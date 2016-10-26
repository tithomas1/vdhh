/*
 * Copyright (C) 2016 Veertu Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#import "VMLibrary.h"
#import "VMImportExport.h"
#import <archive.h>
#import <archive_entry.h>

#include "qemu-common.h"
#include "qemu/option.h"
#include "qemu/error-report.h"
#include "qemu/osdep.h"
#include "sysemu.h"
#include "emublock-backend.h"
#include "block_int.h"
#include "blockjob.h"
#include "qapi.h"
#include "img_ops.h"

// dummy function, allowing to fire static initializers in img_lib
extern void img_init();

enum ImgConvertBlockStatus {
    BLK_DATA,
    BLK_ZERO,
    BLK_BACKING_FILE,
};

typedef struct ImgConvertState {
    BlockDriverState **bs;
    BlockBackend **src;
    int64_t *src_sectors;
    int src_cur, src_num;
    int64_t src_cur_offset;
    int64_t total_sectors;
    int64_t current_sector;
    //int64_t allocated_sectors;
    enum ImgConvertBlockStatus status;
    int64_t sector_next_status;
    BlockBackend *target;
    bool has_zero_init;
    bool compressed;
    bool target_has_backing;
    int min_sparse;
    size_t cluster_sectors;
    size_t buf_sectors;
    uint8_t *buf;
} ImgConvertState;

#define BDRV_O_FLAGS BDRV_O_CACHE_WB
#define BDRV_DEFAULT_CACHE "writeback"

@interface VMImportExport()

@property NSString *tmpFolder;
@property NSString *errMsg;

@end

@implementation VMImportExport

static BOOL importing;
static void (^completionHandler)(NSError *error);
static ImgConvertState state;
static NSCondition *convertCond;
static NSString *convertResult;
static NSError  *convertError;

+ (void)initialize {
    img_init();
}


- (id)init
{
    if (self = [super init]) {
        importing = FALSE;
        self.errMsg = nil;
    }
    convertCond = [[NSCondition alloc] init];
    return self;
}

#define	DEFAULT_BYTES_PER_BLOCK	(64*512)

+ (BOOL) createArchive: (NSString *)arch_file withFiles: (NSArray*) files
          withProgress: (NSProgress *) progress andError: (NSError **)e
{
    struct archive *a;
    struct archive *disk;
    struct archive_entry *entry;
    ssize_t len;
    int fd;
    BOOL res = TRUE;
    static char buff[65535*2];
    
    a = archive_write_new();

    archive_write_add_filter_gzip(a);
    archive_write_set_bytes_per_block(a, DEFAULT_BYTES_PER_BLOCK);

    archive_write_set_format_gnutar(a);
    archive_write_set_options(a, "gzip:compression-level=1");
    if (archive_write_open_filename(a, [arch_file UTF8String]) != ARCHIVE_OK) {
        const char *msg_str = archive_error_string(a);
        msg_str = msg_str ? msg_str : "cannot write archive header";
        NSString *msg = [NSString stringWithUTF8String:msg_str];
        NSLog(@"%@", msg);
        *e = [NSError errorWithDomain: msg
                                 code: archive_errno(disk)
                             userInfo: nil];
        return FALSE;
    }

    disk = archive_read_disk_new();
    archive_read_disk_set_standard_lookup(disk);

    for (NSString *file in files) {
        struct archive *disk = archive_read_disk_new();
        int r;
        
        r = archive_read_disk_open(disk, [file UTF8String]);
        if (r != ARCHIVE_OK) {
            const char *msg = archive_error_string(disk);
            if (!msg)
                msg = "Undefined error";
            NSLog(@"%s", msg);
            *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(disk) userInfo: nil];
            res = FALSE;
            break;
        }
        for (;;) {
            entry = archive_entry_new();
            r = archive_read_next_header2(disk, entry);
            
            if (r == ARCHIVE_EOF)
                break;
            if (r != ARCHIVE_OK) {
                const char *msg = archive_error_string(disk);
                if (!msg)
                    msg = "Undefined error";
                NSLog(@"%s", msg);
                *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(disk) userInfo: nil];
                res = FALSE;
                break;
            }
            archive_read_disk_descend(disk);
            archive_entry_set_pathname(entry, [[file lastPathComponent] UTF8String]);
            r = archive_write_header(a, entry);
            if (r < ARCHIVE_OK) {
                const char *msg_str = archive_error_string(a);
                msg_str = msg_str ? msg_str : "cannot write archive header";
                NSString *msg = [NSString stringWithUTF8String:msg_str];
                NSLog(@"%@", msg);
                *e = [NSError errorWithDomain: msg
                                     code: archive_errno(disk)
                                     userInfo: nil];
                res = FALSE;
                break;
            }
            /* For now, we use a simpler loop to copy data
             * into the target archive. */
            fd = open(archive_entry_sourcepath(entry), O_RDONLY);
            len = read(fd, buff, sizeof(buff));
            while (len > 0) {
                if ([progress isCancelled]) {
                    archive_write_fail(a);
                    res = FALSE;
                    break;
                }
                if (archive_write_data(a, buff, len) < 0) {
                    const char *msg = archive_error_string(a);
                    if (!msg)
                        msg = "Undefined error";
                    NSLog(@"%s", msg);
                    *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
                    res = FALSE;
                    break;
                }
                len = read(fd, buff, sizeof(buff));
                while (len > 0) {
                    if (archive_write_data(a, buff, len) < 0) {
                        const char *msg = archive_error_string(a);
                        if (!msg)
                            msg = "Undefined error";
                        NSLog(@"%s", msg);
                        *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
                        res = FALSE;
                        break;
                    }
                    len = read(fd, buff, sizeof(buff));
                    progress.completedUnitCount = archive_filter_bytes(a, 0) + 1;
                    if ([progress isCancelled]) {
                        archive_write_fail(a);
                        res = FALSE;
                        break;
                    }
                }
            }
            close(fd);
            archive_entry_free(entry);
        }
        archive_read_close(disk);
        archive_read_free(disk);
    }

    archive_write_close(a);
    archive_write_free(a);
    return res;
}

+ (BOOL) extractArchive: (NSString *)arch_file toFolder: (NSString *) folder
          withProgress: (NSProgress *) progress andError: (NSError **)e
{
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;
    BOOL res = TRUE;
    
    if (![[NSFileManager defaultManager] changeCurrentDirectoryPath: folder]) {
         *e = [NSError errorWithDomain: @"Failed to change directory" code: -1 userInfo: nil];
        return FALSE;
    }

    a = archive_read_new();
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
    archive_read_support_filter_gzip(a);
    archive_read_support_format_gnutar(a);
    archive_read_support_format_tar(a);

    archive_write_disk_set_standard_lookup(ext);

    if ((r = archive_read_open_filename(a, [arch_file UTF8String], 10240))) {
        const char *msg = archive_error_string(a);
        if (!msg)
            msg = "Undefined error";
        NSLog(@"%s", msg);
        *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
        return FALSE;
    }

    for (;;) {
        if ([progress isCancelled]) {
            res = FALSE;
            break;
        }
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK) {
            const char *msg = archive_error_string(a);
            if (!msg)
                msg = "Undefined error";
            NSLog(@"%s", msg);
            *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
            res = FALSE;
            break;
        }

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            const char *msg = archive_error_string(a);
            if (!msg)
                msg = "Undefined error";
            NSLog(@"%s", msg);
            *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
            res = FALSE;
            break;
        }

        const void *buff;
        size_t size;
        int64_t offset;

        for (;;) {
            r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                break;
            if (r != ARCHIVE_OK) {
                const char *msg = archive_error_string(a);
                if (!msg)
                    msg = "Undefined error";
                NSLog(@"%s", msg);
                *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
                res = FALSE;
                break;
            }
            r = archive_write_data_block(ext, buff, size, offset);
            if (r < 0) {
                const char *msg = archive_error_string(a);
                if (!msg)
                    msg = "Undefined error";
                NSLog(@"%s", msg);
                *e = [NSError errorWithDomain: [NSString stringWithUTF8String: msg] code: archive_errno(a) userInfo: nil];
                res = FALSE;
                break;
            }
            progress.completedUnitCount = archive_filter_bytes(a, -1) + 1;
            if ([progress isCancelled]) {
                res = FALSE;
                break;
            }
        }
        //archive_entry_free(entry);
    }
    archive_read_close(a);
    archive_read_free(a);
    if (!res)
        archive_write_fail(ext);
    archive_write_close(ext);
    archive_write_free(ext);
    return res;
}

+ (uint64_t) totalBytesInFiles: (NSArray *)files
{
    uint64_t total = 0;
    for (NSString* f in files) {
        struct stat st;

        if (stat([f UTF8String], &st) == 0)
            total += st.st_size;
    }
    return total;
}

- (void) exportVm: (NSString *)vm_name toFile: (NSString *) file format: (VMExportFormat) fmt
       completion: (void(^)(NSError *error))completionHandler
{
    NSProgress *overallProgress = [NSProgress currentProgress];
    NSProgress *progress = [NSProgress progressWithTotalUnitCount:-1];
    NSString *targetDir = nil;
    NSString *targetFile = file;
    VMLibrary *vmlib = [VMLibrary sharedVMLibrary];
    VM *vm = [vmlib readVmProperties: vm_name];
    if (!vm) {
        completionHandler([NSError errorWithDomain: @"VM not found" code: -1 userInfo: nil]);
        return;
    }
    if (ExportFormatVagrantBox == fmt) {
        NSURL *directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:
                                                      [[NSProcessInfo processInfo] globallyUniqueString]] isDirectory:YES];
        [[NSFileManager defaultManager] createDirectoryAtURL:directoryURL withIntermediateDirectories:YES
                                                  attributes:nil error: nil];
        targetDir  = directoryURL.path;
        targetFile = [targetDir stringByAppendingPathComponent: @"box.vmz"];
    }

    NSString *vm_path = [vmlib getVmFolder: vm_name];
    NSMutableArray *vm_files = [NSMutableArray array];
    [vm_files addObject: [vm_path stringByAppendingPathComponent:@"settings.plist"]];
    for (HWHd *hd in vm.hw.hd)  {
        NSString *img = hd.file;
        if (img) {
            if ([[img lastPathComponent] isEqualToString: img])
                img = [[vmlib getVmFolder: vm_name] stringByAppendingPathComponent: img];
        }
        [vm_files addObject: img];
    }

    //NSProgress *progress = [NSProgress currentProgress];
    //[overallProgress becomeCurrentWithPendingUnitCount: 1];
    
    progress.totalUnitCount = [VMImportExport totalBytesInFiles: vm_files];
    //NSProgress *progress = [NSProgress progressWithTotalUnitCount: [self totalBytesInFiles: vm_files]];
    progress.cancellable = YES;
    progress.pausable = NO;

    dispatch_queue_t backgroundQueue = dispatch_queue_create("import/export queue", DISPATCH_QUEUE_SERIAL);
    dispatch_async(backgroundQueue, ^{
        NSError *e = nil;
        BOOL res = [VMImportExport createArchive: targetFile withFiles: vm_files withProgress: progress andError: &e];
        if (!res && ExportFormatVagrantBox == fmt) {
            [[NSFileManager defaultManager] removeItemAtPath:targetDir error: nil];
        }
        else if (res && ExportFormatVagrantBox == fmt) {
            dispatch_sync(dispatch_get_main_queue(), ^{
                [overallProgress becomeCurrentWithPendingUnitCount: 1];
            });
            NSString *metadata_file = [targetDir stringByAppendingPathComponent: @"metadata.json"];
            NSString *metadata = @"{\"provider\": \"veertu\"}";
            [[NSFileManager defaultManager] createFileAtPath: metadata_file
                    contents: [metadata dataUsingEncoding:NSUTF8StringEncoding] attributes:nil];
            
            NSArray *files = @[metadata_file, targetFile];
            __block NSProgress *progress2 = nil;
    
            dispatch_sync(dispatch_get_main_queue(), ^{
                progress2 = [NSProgress progressWithTotalUnitCount: [VMImportExport totalBytesInFiles: files]];
                [overallProgress resignCurrent];
            });
    
            res = [VMImportExport createArchive: file withFiles: files withProgress: progress2 andError: &e];
        }

        if (completionHandler) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([progress isCancelled] || !res)
                    [[NSFileManager defaultManager] removeItemAtPath: file error: nil];
                completionHandler(e);
            });
        }
    });
}

- (NSError *) importVmFromVmz: (NSString *)file toFolder: (NSString *) folder
                withProgress:(NSProgress *)parent
{
    NSProgress *progress;
    [parent becomeCurrentWithPendingUnitCount: 1];
    progress = [NSProgress progressWithTotalUnitCount:[VMImportExport totalBytesInFiles:
                                                       [NSArray arrayWithObjects: file, nil]]];
    progress.cancellable = YES;
    progress.pausable = NO;
    [parent resignCurrent];

    NSError *e = nil;
    BOOL res = [VMImportExport extractArchive: file toFolder: folder withProgress: progress andError: &e];

    if ([progress isCancelled] || !res) {
        if ([[NSFileManager defaultManager] fileExistsAtPath: folder isDirectory: nil])
            [[NSFileManager defaultManager] removeItemAtPath:folder error: nil];
    }
    return e;
}

#define IO_BUF_SIZE (2 * 1024 * 1024)

static int64_t sectors_to_bytes(int64_t sectors)
{
    return sectors << BDRV_SECTOR_BITS;
}

static int64_t sectors_to_process(int64_t total, int64_t from)
{
    return MIN(total - from, IO_BUF_SIZE >> BDRV_SECTOR_BITS);
}

static BlockBackend *img_open(const char *id, const char *filename,
                              const char *fmt, int flags,
                              bool require_io, bool quiet)
{
    BlockBackend *blk;
    BlockDriverState *bs;
    BlockDriver *drv;
    Error *local_err = NULL;
    int ret;

    img_ops_init();
    blk = blk_new_with_bs(id, &error_abort);
    bs = blk_bs(blk);
    
    if (fmt) {
        drv = bdrv_find_format(fmt);
        if (!drv) {
            //error_report("Unknown file format '%s'", fmt);
            goto fail;
        }
    } else {
        drv = NULL;
    }
    
    ret = bdrv_open(&bs, filename, NULL, NULL, flags, drv, &local_err);
    if (ret < 0) {
        //error_report("Could not open '%s': %s", filename, error_get_pretty(local_err));
        //error_free(local_err);
        goto fail;
    }
    
    if (bdrv_is_encrypted(bs) && require_io)
        goto fail;

    return blk;
fail:
    blk_unref(blk);
    return NULL;
}

static void convert_select_part(ImgConvertState *s, int64_t sector_num)
{
    assert(sector_num >= s->src_cur_offset);
    while (sector_num - s->src_cur_offset >= s->src_sectors[s->src_cur]) {
        s->src_cur_offset += s->src_sectors[s->src_cur];
        s->src_cur++;
        assert(s->src_cur < s->src_num);
    }
}

#define BDRV_REQUEST_MAX_SECTORS MIN(SIZE_MAX >> BDRV_SECTOR_BITS, INT_MAX >> BDRV_SECTOR_BITS)

static int convert_iteration_sectors(ImgConvertState *s, int64_t sector_num)
{
    int64_t ret;
    int n;
    
    convert_select_part(s, sector_num);
    
    assert(s->total_sectors > sector_num);
    n = MIN(s->total_sectors - sector_num, BDRV_REQUEST_MAX_SECTORS);
    
    if (s->sector_next_status <= sector_num) {
        ret = bdrv_get_block_status(blk_bs(s->src[s->src_cur]),
                                    sector_num - s->src_cur_offset,
                                    n, &n);
        if (ret < 0) {
            return ret;
        }
        
        if (ret & BDRV_BLOCK_ZERO) {
            s->status = BLK_ZERO;
        } else if (ret & BDRV_BLOCK_DATA) {
            s->status = BLK_DATA;
        } else if (!s->target_has_backing) {
            /* Without a target backing file we must copy over the contents of
             * the backing file as well. */
            /* TODO Check block status of the backing file chain to avoid
             * needlessly reading zeroes and limiting the iteration to the
             * buffer size */
            s->status = BLK_DATA;
        } else {
            s->status = BLK_BACKING_FILE;
        }
        
        s->sector_next_status = sector_num + n;
    }
    
    n = MIN(n, s->sector_next_status - sector_num);
    if (s->status == BLK_DATA) {
        n = MIN(n, s->buf_sectors);
    }
    
    /* We need to write complete clusters for compressed images, so if an
     * unallocated area is shorter than that, we must consider the whole
     * cluster allocated. */
    if (s->compressed) {
        if (n < s->cluster_sectors) {
            n = MIN(s->cluster_sectors, s->total_sectors - sector_num);
            s->status = BLK_DATA;
        } else {
            n = QEMU_ALIGN_DOWN(n, s->cluster_sectors);
        }
    }
    
    return n;
}

static int convert_read(ImgConvertState *s, int64_t sector_num, int nb_sectors,
                        uint8_t *buf)
{
    int n;
    int ret;
    
    if (s->status == BLK_ZERO || s->status == BLK_BACKING_FILE) {
        return 0;
    }

    assert(nb_sectors <= s->buf_sectors);
    while (nb_sectors > 0) {
        BlockBackend *blk;
        int64_t bs_sectors;
        
        /* In the case of compression with multiple source files, we can get a
         * nb_sectors that spreads into the next part. So we must be able to
         * read across multiple BDSes for one convert_read() call. */
        convert_select_part(s, sector_num);
        blk = s->src[s->src_cur];
        bs_sectors = s->src_sectors[s->src_cur];
        
        n = MIN(nb_sectors, bs_sectors - (sector_num - s->src_cur_offset));

        ret = blk_read(blk, sector_num - s->src_cur_offset, buf, n);
        if (ret < 0)
            return ret;
        
        sector_num += n;
        nb_sectors -= n;
        buf += n * BDRV_SECTOR_SIZE;
    }
    
    return 0;
}

static int is_allocated_sectors(const uint8_t *buf, int n, int *pnum)
{
    bool is_zero;
    int i;
    
    if (n <= 0) {
        *pnum = 0;
        return 0;
    }
    is_zero = buffer_is_zero(buf, 512);
    for(i = 1; i < n; i++) {
        buf += 512;
        if (is_zero != buffer_is_zero(buf, 512)) {
            break;
        }
    }
    *pnum = i;
    return !is_zero;
}

static int is_allocated_sectors_min(const uint8_t *buf, int n, int *pnum,
                                    int min)
{
    int ret;
    int num_checked, num_used;
    
    if (n < min) {
        min = n;
    }
    
    ret = is_allocated_sectors(buf, n, pnum);
    if (!ret) {
        return ret;
    }
    
    num_used = *pnum;
    buf += BDRV_SECTOR_SIZE * *pnum;
    n -= *pnum;
    num_checked = num_used;
    
    while (n > 0) {
        ret = is_allocated_sectors(buf, n, pnum);
        
        buf += BDRV_SECTOR_SIZE * *pnum;
        n -= *pnum;
        num_checked += *pnum;
        if (ret) {
            num_used = num_checked;
        } else if (*pnum >= min) {
            break;
        }
    }
    
    *pnum = num_used;
    return 1;
}

static int convert_write(ImgConvertState *s, int64_t sector_num, int nb_sectors,
                         const uint8_t *buf)
{
    int ret;
    
    while (nb_sectors > 0) {
        int n = nb_sectors;
        
        switch (s->status) {
            case BLK_BACKING_FILE:
                /* If we have a backing file, leave clusters unallocated that are
                 * unallocated in the source image, so that the backing file is
                 * visible at the respective offset. */
                assert(s->target_has_backing);
                break;
                
            case BLK_DATA:
                /* We must always write compressed clusters as a whole, so don't
                 * try to find zeroed parts in the buffer. We can only save the
                 * write if the buffer is completely zeroed and we're allowed to
                 * keep the target sparse. */
                if (s->compressed) {
                    if (s->has_zero_init && s->min_sparse &&
                        buffer_is_zero(buf, n * BDRV_SECTOR_SIZE))
                    {
                        assert(!s->target_has_backing);
                        break;
                    }
                    
                    ret = blk_write_compressed(s->target, sector_num, buf, n);
                    if (ret < 0) {
                        return ret;
                    }
                    break;
                }
                
                /* If there is real non-zero data or we're told to keep the target
                 * fully allocated (-S 0), we must write it. Otherwise we can treat
                 * it as zero sectors. */
                if (!s->min_sparse ||
                    is_allocated_sectors_min(buf, n, &n, s->min_sparse))
                {
                    ret = blk_write(s->target, sector_num, buf, n);
                    if (ret < 0) {
                        return ret;
                    }
                    break;
                }
                /* fall-through */
                
            case BLK_ZERO:
                if (s->has_zero_init) {
                    break;
                }
                ret = blk_write_zeroes(s->target, sector_num, n, 0);
                if (ret < 0) {
                    return ret;
                }
                break;
        }
        
        sector_num += n;
        nb_sectors -= n;
        buf += n * BDRV_SECTOR_SIZE;
    }
    
    return 0;
}

- (uint64_t) timestamp
{
    struct timeval ts;
    gettimeofday(&ts, NULL);
    return  ts.tv_sec*1000LL + ts.tv_usec / 1000;
}

- (void) doConvert: (NSProgress *) progress
{
    ImgConvertState *s = &state;
    int n = 0;
    int ret = 0;
    bool finished = false;
    int loops = 0;

    while (s->current_sector < s->total_sectors) {
        progress.completedUnitCount = s->current_sector + 1;
        uint64_t start = [self timestamp];

        n = convert_iteration_sectors(s, s->current_sector);
        if (n < 0) {
            ret = n;
            break;
        }

        ret = convert_read(s, s->current_sector, n, state.buf);
        if (ret < 0) {
            //error_report("error while reading sector %" PRId64": %s", sector_num, strerror(-ret));
            break;
        }
        
        ret = convert_write(s, s->current_sector, n, state.buf);
        if (ret < 0) {
            //error_report("error while writing sector %" PRId64  ": %s", sector_num, strerror(-ret));
            break;
        }
        
        s->current_sector += n;
        if ([self timestamp] - start > 2 || loops++ > 1000)
            break;
    }

    finished = (s->current_sector == s->total_sectors) || [progress isCancelled] || ret;
    if (!finished)
        [self performSelectorOnMainThread: @selector(doConvert:) withObject:progress waitUntilDone: FALSE];
fail:
    if (finished) {
        if (s->compressed && !ret)
            ret = blk_write_compressed(s->target, 0, NULL, 0);
        [self finishConvert: ret cancelled: [progress isCancelled]];
    }
}

- (NSError *) startConvertWithProgress:(NSProgress *)progress
{
    int ret = 0;
    ImgConvertState *s = &state;

    progress.totalUnitCount = s->total_sectors;
    progress.cancellable = YES;
    progress.pausable = NO;

    /* Check whether we have zero initialisation or can get it efficiently */
    s->has_zero_init = s->min_sparse && !s->target_has_backing ? bdrv_has_zero_init(blk_bs(s->target)) : false;
    
    if (!s->has_zero_init && !s->target_has_backing &&
        bdrv_can_write_zeroes_with_unmap(blk_bs(s->target))) {
        ret = bdrv_make_zero(blk_bs(s->target), BDRV_REQ_MAY_UNMAP);
        if (ret == 0) {
            s->has_zero_init = true;
        }
    }
    
    /* Allocate buffer for copied data. For compressed images, only one cluster
     * can be copied at a time. */
    if (s->compressed) {
        if (s->cluster_sectors <= 0 || s->cluster_sectors > s->buf_sectors) {
            //error_report("invalid cluster size");
            ret = -EINVAL;
            convertError = [NSError errorWithDomain:@"Conversion error" code:ret userInfo:nil];
            goto fail;
        }
        s->buf_sectors = s->cluster_sectors;
    }
    state.buf = blk_blockalign(s->target, s->buf_sectors * BDRV_SECTOR_SIZE);

    // Do the copy
    s->src_cur = 0;
    s->src_cur_offset = 0;
    s->sector_next_status = 0;
    
    s->current_sector = 0;
    
    [self performSelectorOnMainThread: @selector(doConvert:) withObject:progress waitUntilDone: FALSE];
    ret = 0;

fail:
    if (ret < 0)
        [self finishConvert: ret cancelled: FALSE];
    return convertError;
}

- (void) finishConvert: (int) res cancelled: (BOOL) cancelled
{
    [self importVmCleanState];
    if (res || cancelled) {
        if ([[NSFileManager defaultManager] fileExistsAtPath: self.tmpFolder isDirectory: nil])
            [[NSFileManager defaultManager] removeItemAtPath: self.tmpFolder error: nil];
    }
    if (res)
        convertError = [NSError errorWithDomain: self.errMsg ? self.errMsg : @"Conversion error" code: res userInfo: nil];

    self.tmpFolder = nil;
    convertResult = @"";
    [convertCond signal];
}

- (void) importVmCleanState
{
    if (!importing)
        return;
    importing = FALSE;

    free(state.buf);
    blk_unref(state.target);
    g_free(state.bs);
    if (state.src) {
        blk_unref(state.src[0]);
        g_free(state.src);
    }
    g_free(state.src_sectors);
    memset(&state, 0, sizeof(state));
}

- (void) _importVmFromVmdk: (NSString *)file toFolder: (NSString *) folder withProgress:(NSProgress *)progress
{
    if (![[NSFileManager defaultManager] changeCurrentDirectoryPath: folder]) {
        convertError = [NSError errorWithDomain: @"Failed to change directory" code: -1 userInfo: nil];
        [convertCond signal];
        return;
    }
    [self importVmCleanState];
    importing = TRUE;
    
    int bs_n = 0, bs_i, compress = 0, cluster_sectors;
    int ret = 0;
    int  flags, src_flags;
    const char *fmt = NULL, *out_fmt, *cache, *src_cache, *out_baseimg, *out_filename;
    BlockDriver *drv, *proto_drv;
    BlockBackend **blk = NULL, *out_blk = NULL;
    BlockDriverState **bs = NULL, *out_bs = NULL;
    int64_t total_sectors;
    int64_t *bs_sectors = NULL;
    size_t bufsectors = IO_BUF_SIZE / BDRV_SECTOR_SIZE;
    BlockDriverInfo bdi;
    QemuOpts *opts = NULL;
    QemuOptsList *create_opts = NULL;
    int min_sparse = 8; /* Need at least 4k of zeros for sparse detection */
    bool quiet = false;
    Error *local_err = NULL;

    out_fmt = "qcow2";
    cache = "unsafe";
    src_cache = BDRV_DEFAULT_CACHE;
    out_baseimg = NULL;

    out_filename = "hd0.img";
    src_flags = BDRV_O_FLAGS;
    ret = bdrv_parse_cache_flags(src_cache, &src_flags);
    if (ret < 0)
        goto out;

    bs_n = 1;
    blk = g_new0(BlockBackend *, bs_n);
    bs = g_new0(BlockDriverState *, bs_n);
    bs_sectors = g_new(int64_t, bs_n);
    
    total_sectors = 0;
    for (bs_i = 0; bs_i < bs_n; bs_i++) {
        char *id = bs_n > 1 ? g_strdup_printf("source_%d", bs_i) : g_strdup("source");
        blk[bs_i] = img_open(id, [file UTF8String], fmt, src_flags, true, quiet);
        g_free(id);
        if (!blk[bs_i]) {
            ret = -1;
            self.errMsg = @"Failed opening image file";
            goto out;
        }
        bs[bs_i] = blk_bs(blk[bs_i]);
        bs_sectors[bs_i] = bdrv_nb_sectors(bs[bs_i]);
        if (bs_sectors[bs_i] < 0) {
            //error_report("Could not get size of %s: %s", [file UTF8String], strerror(-bs_sectors[bs_i]));
            self.errMsg = @"Could not read image size";
            ret = -1;
            goto out;
        }
        total_sectors += bs_sectors[bs_i];
    }

    /* Find driver and parse its options */
    drv = bdrv_find_format(out_fmt);
    if (!drv) {
        self.errMsg = @"Unrecognized image format";
        ret = -1;
        goto out;
    }
    
    proto_drv = bdrv_find_protocol(out_filename, true);
    if (!proto_drv) {
        //error_report_err(local_err);
        ret = -1;
        goto out;
    }
    
    if (!drv->create_opts || !proto_drv->create_opts) {
        ret = -1;
        goto out;
    }

    create_opts = vmx_opts_append(create_opts, drv->create_opts);
    create_opts = vmx_opts_append(create_opts, proto_drv->create_opts);
    opts = vmx_opts_create(create_opts, NULL, 0, &error_abort);
    vmx_opt_set_number(opts, BLOCK_OPT_SIZE, total_sectors * 512);

    ret = bdrv_create(drv, out_filename, opts, &local_err);
    if (ret < 0) {
        self.errMsg = @"Failed to create Veertu VM image";
        goto out;
    }

    flags = min_sparse ? (BDRV_O_RDWR | BDRV_O_UNMAP) : BDRV_O_RDWR;
    ret = bdrv_parse_cache_flags(cache, &flags);
    if (ret < 0) {
        //error_report("Invalid cache option: %s", cache);
        goto out;
    }
    
    out_blk = img_open("target", out_filename, out_fmt, flags, true, quiet);
    if (!out_blk) {
        ret = -1;
        self.errMsg = @"Failed to create Veertu VM image";
        goto out;
    }
    out_bs = blk_bs(out_blk);

    bufsectors = MIN(32768,
                     MAX(bufsectors, MAX(out_bs->bl.opt_transfer_length,
                                         out_bs->bl.discard_alignment))
                     );

    cluster_sectors = 0;
    ret = bdrv_get_info(out_bs, &bdi);
    if (ret < 0) {
        if (compress) {
            self.errMsg = @"Could not read block driver info";
            goto out;
        }
    } else {
        compress = compress || bdi.needs_compressed_writes;
        cluster_sectors = bdi.cluster_size / BDRV_SECTOR_SIZE;
    }

    state = (ImgConvertState) {
            .bs                 = bs,
            .src                = blk,
            .src_sectors        = bs_sectors,
            .src_num            = bs_n,
            .total_sectors      = total_sectors,
            .current_sector     = 0,
            .target             = out_blk,
            .compressed         = compress,
            .target_has_backing = (bool) out_baseimg,
            .min_sparse         = min_sparse,
            .cluster_sectors    = cluster_sectors,
            .buf_sectors        = bufsectors,
            .buf                = NULL,
    };
    if ((ret = [self startConvertWithProgress:progress] < 0))
        [self finishConvert: ret cancelled: FALSE];
   
    return;

out:
    blk_unref(out_blk);
    g_free(bs);
    if (blk) {
        for (bs_i = 0; bs_i < bs_n; bs_i++) {
            blk_unref(blk[bs_i]);
        }
        g_free(blk);
    }
    g_free(bs_sectors);
    [self finishConvert: ret cancelled: FALSE];
    
}

- (NSError *) importVmFromVmdk: (NSString *)file toFolder: (NSString *) folder withProgress:(NSProgress *)progress
{
    self.tmpFolder = folder;
    [convertCond lock];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self _importVmFromVmdk: file toFolder: folder withProgress:progress];
    });
    while (!convertResult && !convertError)
        [convertCond wait];
    [convertCond unlock];
    return convertError;
}

- (void) untarVm: (NSString *)file toFolder: (NSString *) folder
    withProgress:(NSProgress *)progress
      completion: (void(^)(NSError *error))completionHandler
{
    dispatch_queue_t backgroundQueue = dispatch_queue_create("import/export queue", DISPATCH_QUEUE_SERIAL);
    dispatch_async(backgroundQueue, ^{
        NSError *e = nil;
        BOOL res = [VMImportExport extractArchive: file toFolder: folder withProgress: progress andError: &e];
        
        if (completionHandler) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([progress isCancelled] || !res) {
                    if ([[NSFileManager defaultManager] fileExistsAtPath: folder isDirectory: nil])
                        [[NSFileManager defaultManager] removeItemAtPath:folder error: nil];
                }
                progress.completedUnitCount = progress.totalUnitCount;
                completionHandler(e);
            });
        }
    });
}

@end
