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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "img_ops.h"
#include "utils.h"
#import "SystemInfo.h"
#import "VMImportExport.h"
#import "VM.h"

@import AppKit;

#define VM_LIB_DIR  @"VM Library"
#define DEFAULT_SCSI_MODEL @"megasas"
#define DEFAULT_NIC_MODEL @"e1000"

@interface NSTask(vlaunch)

+ (pid_t)vlaunchTaskWithLaunchPath:(NSString *)path arguments:(NSArray<NSString *> *)args;

@end


@interface VMLibrary ()

//@property NSDictionary *temp_vm;
@property NSString *temp_vm_name;
@property NSString *temp_vm_folder;

@property NSDictionary *os_list;
@property NSDictionary *scsi_list;
@property NSDictionary *nic_list;

- (NSString *) getVmPropertiesFileName:(NSString*)name;

@end

@implementation VMLibrary {
    NSMutableDictionary<NSString *, NSNumber *> *vmstate;
    NSLock *vmstate_lock;
}

+ (instancetype) sharedVMLibrary
{
    static VMLibrary *sharedLib = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedLib = [[self alloc] init];
    });
    return sharedLib;
}

- (instancetype)init
{
    vmstate = [NSMutableDictionary dictionary];
    vmstate_lock = [[NSLock alloc] init];

    if (self = [super init]) {
        BOOL vmlib_found = FALSE;
        NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
        if ([prefs objectForKey: @"VMLIB_PATH"]) {
            self.vmLibPath = [prefs objectForKey: @"VMLIB_PATH"];

            NSData *bookmark = [prefs objectForKey: self.vmLibPath];
            if (bookmark) {
                BOOL isStale;
                NSError *error;
                NSURL* outUrl = [NSURL URLByResolvingBookmarkData: bookmark options:NSURLBookmarkResolutionWithSecurityScope
                        relativeToURL:nil bookmarkDataIsStale: &isStale error:&error];
                [outUrl stopAccessingSecurityScopedResource];
                if (![outUrl startAccessingSecurityScopedResource]) {
                    NSAlert *alert = [[NSAlert alloc] init];
                    [alert setMessageText: NSLocalizedString(@"VMLIB_ACCESS_DENIED", nil)];
                    [alert setInformativeText:NSLocalizedString(@"VMLIB_ACCESS_DENIED_INFO", nil)];
                    [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
                    alert.alertStyle = NSCriticalAlertStyle;
                    [alert runModal];
                } else
                    vmlib_found = TRUE;
            }
        }
        if (!vmlib_found)
            self.vmLibPath = [NSString stringWithFormat:@"%@/%@", NSHomeDirectory(), VM_LIB_DIR];

        NSMutableDictionary *os = [NSMutableDictionary dictionary];
        [os setObject: [NSArray arrayWithObjects:
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows 10", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows 8", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows Server 2012", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin71:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows 7", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin71:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows Server 2008", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin71:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows Vista", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWin71:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Windows XP", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigWinXp:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Other", @"name",
                         NSStringFromSelector(@selector(defaultVMConfigIDE:)), @"cfg", nil], nil]
               forKey: @"Windows"];
        [os setObject: [NSArray arrayWithObjects:
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Ubuntu 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Ubuntu 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Debian 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Debian 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"CentOS 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"CentOS 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Mint 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Mint 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"SUSE 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"SUSE 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigSCSI:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Other 64bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigIDE:)), @"cfg", nil],
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Other 32bit", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigIDE:)), @"cfg", nil], nil]
           forKey: @"Linux"];

        [os setObject: [NSArray arrayWithObjects:
                        [NSDictionary dictionaryWithObjectsAndKeys: @"Other", @"name",
                                        NSStringFromSelector(@selector(defaultVMConfigIDE:)), @"cfg", nil], nil]
            forKey: @"Other"];

        self.os_list = os;

        self.scsi_list = [NSDictionary dictionaryWithObjectsAndKeys:
                          /*@"LSI SAS 1068", @"lsisas1068",*/ @"LSI MegaSAS", @"megasas", nil];
        self.nic_list = [NSDictionary dictionaryWithObjectsAndKeys:
                          @"e1000", @"e1000", @"rtl8139", @"rtl8139", nil];
    }
    return self;
}

- (NSString *) getVmPropertiesFileName:(NSString*)vm_name
{
    return [[self getVmFolder:vm_name] stringByAppendingPathComponent:@"settings.plist"];
}

- (int) maxVmMemoryMB
{
    return 16 * 1024; // 16GB
}

- (int) minVmMemoryMB
{
    return 128; // 128 MB
}

- (int) maxVmHDSizeMB
{
    return 1024 * 1024 * 10;
}
- (int) minVmHDSizeMB
{
    return 10;
}

- (NSArray *) getSCSIModels
{
    return [self.scsi_list allKeys];
}

- (NSString*) getSCSIDisplayName: (NSString*)model
{
    NSString *name = [self.scsi_list objectForKey: model];
    return name ? name : model;
}

- (NSArray *) getNicModels
{
    return [self.nic_list allKeys];
}

- (NSString*) getNicDisplayName: (NSString*)model
{
    return [self.nic_list objectForKey: model];
}

- (NSArray*) getOSList
{
    return [self.os_list allKeys];
}

- (NSArray*) getOSFamilyList: (NSString*) os
{
    NSMutableArray *f = [NSMutableArray array];

    NSArray *dlist = [self.os_list objectForKey: os];
    for (NSDictionary *family in dlist) {
        [f addObject: [family objectForKey: @"name"]];
    }
    return [NSArray arrayWithArray: f];
}

- (void)dealloc
{
    // Should never be called, but just here for clarity really.
    NSAssert(FALSE, @"Singleton deallocated :[");
}

- (bool) firstTimeSetup
{
    BOOL is_dir;
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: self.vmLibPath isDirectory: &is_dir];

    if (!exists) {
        NSError *error = nil;
        if ([[NSFileManager defaultManager] createDirectoryAtPath: self.vmLibPath withIntermediateDirectories: TRUE attributes: nil error: &error])
            exists = TRUE;
        else
            NSLog(@"Failed to create dir: %@", error);
    }
    return exists;
}

- (NSArray*) getVmList
{
    NSMutableArray *vms = [NSMutableArray array];

    NSDirectoryEnumerator *dirEnum = [[NSFileManager defaultManager] enumeratorAtURL:
            [NSURL fileURLWithPath: self.vmLibPath]
            includingPropertiesForKeys: @[NSURLNameKey, NSURLIsDirectoryKey]
            options: NSDirectoryEnumerationSkipsHiddenFiles | NSDirectoryEnumerationSkipsSubdirectoryDescendants
            errorHandler: nil];

    for (NSURL *url in dirEnum) {
        NSNumber *isDirectory;
        [url getResourceValue: &isDirectory forKey: NSURLIsDirectoryKey error: nil];
        if ([isDirectory boolValue]) {
            NSString *name;
            [url getResourceValue: &name forKey: NSURLNameKey error: nil];
            if ([self readVmProperties: name])
                [vms addObject: name];
        }
    }

    // sort
    long j = 0;
    NSString *path = [NSString stringWithFormat: @"%@/%@", self.vmLibPath, @".sortkey"];
    NSArray *a = [NSArray arrayWithContentsOfFile: path];
    if (a) {
        for (NSString *vm_name in a) {
            long i = [vms indexOfObject: vm_name];
            if (i != NSNotFound) {
                [vms removeObjectAtIndex: i];
                [vms insertObject: vm_name atIndex: j++];
            }
        }
    }

    return vms;
}

- (BOOL) vmExist:(NSString *) name
{
    NSString *path = [self getVmPropertiesFileName:name];
    return [[NSFileManager defaultManager] fileExistsAtPath:path];
}

- (VM *) readVmProperties: (NSString*) name
{
    NSString* path = [self getVmPropertiesFileName:name];
    NSDictionary *d = [NSDictionary dictionaryWithContentsOfFile: path];
    VM *vm = [[VM withVMLibrary:self] initFromDictionary:d];
    vm.name = name;
    if (0 == [vm.display_name length])
        vm.display_name = [name lastPathComponent];
    return vm;
}

- (void) writeVmProperties:(VM *)vm
{
    assert(vm.name);

    NSString* name = vm.name;
    NSString* path = [self getVmPropertiesFileName:name];
    if (![[vm toDictionary] writeToFile: path atomically: YES])
        [NSException raise:@"Cannot create properties file" format:@"file %@", path];

    // TODO: notify about VMLibrary changed changes
}

- (void) setVmDisplayName: (NSString*) name display: (NSString *) new_name
{
    NSString *path = [self getVmPropertiesFileName:name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    [vm setObject: new_name forKey: @"display_name"];
    [vm writeToFile: path atomically: YES];
}

- (void) setVmCpuInfo: (VM*)vm cores:(NSNumber*) new_cores threads:(NSNumber*) new_threads sockets:(NSNumber*) new_sockets
{
    int max_cpus = [SystemInfo getCpuTotal];

    VMHw *hw = vm.hw;
    HWCpu *cpu = hw.cpu;
    NSNumber *cores = cpu.cores;
    NSNumber *sockets = cpu.sockets;
    NSNumber *threads = cpu.threads;
    if(new_cores == nil) new_cores = cores;
    if(new_threads == nil) new_threads = threads;
    if(new_sockets == nil) new_sockets = sockets;

    if ([threads intValue] != [new_threads intValue]) {
        threads = new_threads;
        if ([cores intValue] * [threads intValue] * [sockets intValue] > max_cpus) {
            sockets = [NSNumber numberWithInt: 1];
            cores = [NSNumber numberWithInt: 1];
        }
    }
    else if ([sockets intValue] != [new_sockets intValue]) {
        sockets = new_sockets;
        if ([cores intValue] * [threads intValue] * [sockets intValue] > max_cpus) {
            threads = [NSNumber numberWithInt: 1];
            cores = [NSNumber numberWithInt: 1];
        }
    }
    else {
        cores = new_cores;
        threads = [NSNumber numberWithInt: 1];
        if ([cores intValue] * [threads intValue] * [sockets intValue] > max_cpus)
            sockets = [NSNumber numberWithInt: 1];
    }

    HWCpu *cpu_info = [[HWCpu alloc] init];
    cpu_info.sockets = sockets;
    cpu_info.threads = threads;
    cpu_info.cores = cores;

    [self _setVmCpuInfo: vm info: cpu_info];
}


- (void) _setVmCpuInfo:(VM*)vm info: (HWCpu *) cpu_info
{
    vm.hw.cpu = [cpu_info copy];
    [self writeVmProperties:vm];
}

- (void) setVmMemory: (VM*)vm memory: (NSString *) ram
{
    vm.hw.ram = ram;
    [self writeVmProperties:vm];
}

- (void) setVGA: (NSString *) vm_name type: (NSString *) type
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *hw = [vm objectForKey: @"hw"];
    [hw setObject: type forKey: @"vga"];

    [vm writeToFile: path atomically: YES];
}

- (void) setCdroms: (VM*)vm cdroms: (NSArray<HWCdrom *> *) cds
{
    vm.hw.cdrom = cds;
    [self writeVmProperties:vm];
}

- (void) setHds: (VM*)vm hds: (NSArray<HWHd *> *) hds
{
    vm.hw.hd = hds;
    [self writeVmProperties:vm];
}

- (void) setAudio: (VM*)vm device: (HWAudio *) audio
{
    vm.hw.audio = audio;
    [self writeVmProperties:vm];
}

- (void) setNics: (VM*)vm nics: (NSArray<HWNic *> *) nics
{
    vm.hw.nic = nics;
    [self writeVmProperties:vm];
}

- (void) setControllers: (VM*)vm controllers: (NSArray<HWDiskController *> *) controllers
{
    vm.hw.disk_controller = controllers;
    [self writeVmProperties:vm];
}

- (void) setDescriptionForVm: (NSString *) vm_name description: (NSString*) desc
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"general"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    [general setObject: desc forKey: @"description"];
    vm[@"general"] = general;

    [vm writeToFile: path atomically: YES];
}

- (void) setBootDeviceForVm: (NSString *) vm_name device: (NSString*) d
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"general"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    [general setObject: d forKey: @"boot_device"];
    vm[@"general"] = general;

    [vm writeToFile: path atomically: YES];
}

- (void) setRemapCmd: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv) {
        adv = [NSMutableDictionary dictionary];
    }
    [adv setObject: [NSNumber numberWithBool: val] forKey: @"remap_cmd"];
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}

- (void) setHeadlessMode: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv) {
        adv = [NSMutableDictionary dictionary];
    }
    [adv setObject: [NSNumber numberWithBool: val] forKey: @"headless"];
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}


- (void) setReadOnlyVM: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv) {
        adv = [NSMutableDictionary dictionary];
    }
    [adv setObject: [NSNumber numberWithBool: val] forKey: @"snapshot"];
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}

- (void) setEnableFS: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv)
        adv = [NSMutableDictionary dictionary];
    NSMutableDictionary *gt = [adv objectForKey: @"guest_tools"];
    if (!gt)
        gt = [NSMutableDictionary dictionary];

    [gt setObject: [NSNumber numberWithBool: val] forKey: @"file_sharing"];
    adv[ @"guest_tools"] = gt;
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}

- (void) setEnableCopyPaste: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv)
        adv = [NSMutableDictionary dictionary];
    NSMutableDictionary *gt = [adv objectForKey: @"guest_tools"];
    if (!gt)
        gt = [NSMutableDictionary dictionary];

    [gt setObject: [NSNumber numberWithBool: val] forKey: @"copy_paste"];
    adv[ @"guest_tools"] = gt;
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}

- (void) setHDPISupport: (NSString*) vm_name value: (BOOL) val
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv) {
        adv = [NSMutableDictionary dictionary];
    }
    [adv setObject: [NSNumber numberWithBool: val] forKey: @"hdpi"];
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];
}

- (void) setFSFolder: (NSString*) vm_name folder: (NSString*) folder
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    NSMutableDictionary *general = [vm objectForKey: @"advanced"];
    if (!general) {
        general = [NSMutableDictionary dictionary];
    }
    NSMutableDictionary *adv = [vm objectForKey: @"advanced"];
    if (!adv)
        adv = [NSMutableDictionary dictionary];
    NSMutableDictionary *gt = [adv objectForKey: @"guest_tools"];
    if (!gt)
        gt = [NSMutableDictionary dictionary];

    [gt setObject: folder forKey: @"fs_folder"];
    adv[ @"guest_tools"] = gt;
    vm[@"advanced"] = adv;

    [vm writeToFile: path atomically: YES];

    // set permissions
    NSData* data = [[NSURL fileURLWithPath: folder] bookmarkDataWithOptions:
            NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
    if (data)
        [prefs setObject:data forKey: folder];
    [prefs synchronize];
}

- (void) recalculateHdSize: (NSString*) vm_name
{
    VM * vm = [self readVmProperties:vm_name];

    NSArray<HWHd *> *hds = vm.hw.hd;
    for (int i = 0; i < [hds count]; i++)  {
        HWHd *hd = [hds objectAtIndex: i];
        if ([self isHdCommited: vm_name hd: [hd.id intValue]]) {
            hd.size = nil;
        }
    }
    [self setHds: vm hds: hds];
}

- (HWDiskController *) findController: (VM *)vm  byId: (int) controller_id
{
    int i;

    VMHw *hw = vm.hw;
    // controller
    NSArray<HWDiskController *> *controllers = hw.disk_controller;
    for (i = 0; i < [controllers count]; i++)  {
        HWDiskController *controller = [controllers objectAtIndex: i];
        NSNumber *cid = controller.id;
        if ([cid intValue] == controller_id)
            return controller;
    }
    return nil;
}

- (int) generateBusId: (VM *) vm controller: (HWDiskController *) controller device: (NSObject *) dev
{
    long cid = [controller.id integerValue];
    NSString *ctype = [controller.type lowercaseString];

    VMHw *hw = vm.hw;
    NSArray<HWHd *> *hds = hw.hd;
    NSArray<HWCdrom *> *cds = hw.cdrom;

    NSMutableSet *busy = [NSMutableSet set];
    NSMutableArray *unassigned_dev = [NSMutableArray array];

    for (HWHd *hd in hds) {
        long hd_cid = [hd.controller integerValue];
        if (hd_cid != cid)
            continue;
        NSNumber *bus_id = hd.bus;
        if (bus_id)
            [busy addObject: bus_id];
        else
            [unassigned_dev addObject: hd];
    }
    for (HWCdrom *cd in cds) {
        long cd_cid = [cd.controller integerValue];
        if (cd_cid != cid)
            continue;
        NSNumber *bus_id = cd.bus;
        if (bus_id)
            [busy addObject: bus_id];
        else
            [unassigned_dev addObject: cd];
    }

    // can attach up to 16 devices to a scsi controller
    if ([ctype isEqualToString: @"scsi"]) {
        /*int dev_i = 0;
        for (int i = 0; i < 16 && dev_i < [unassigned_dev count]; i++) {
            if ([busy containsObject: [NSNumber numberWithInt: i]])
                continue;

            NSDictionary *dev1 = [unassigned_dev objectAtIndex: dev_i];
            if ([dev1 isEqualTo: dev])
                return i;
            dev_i++;
        }*/
        return 0;
    } else if ([ctype isEqualToString: @"ide"]) {
        int dev_i = 0;
        for (int i = 0; i < 2 && dev_i < [unassigned_dev count]; i++) {
            if ([busy containsObject: [NSNumber numberWithInt: i]])
                continue;

            NSObject *dev1 = [unassigned_dev objectAtIndex: dev_i];
            if ([[dev class] isEqual: [dev1 class]] &&
                [[dev1 valueForKey:@"id"] isEqual: [dev valueForKey:@"id"]])
                return i;
            dev_i++;
        }
    }
    return -1;
}

- (BOOL)createUSBParam:(VMHw*)hw parameters:(NSMutableArray*)params error:(NSError**)error
{
    // create USB controllers
    NSMutableDictionary<NSString*, NSString*>* busMap = [NSMutableDictionary dictionary];

    if (0 == [hw.uhc count]) {
        // create default UHC (must be in sync with model, created with [VMHw createNewUhc:])
        [params addObjectsFromArray:@[@"-device", @"usb-ehci,id=uhc0"]];
    }
    else {
        int index = 0;
        for (HWUhc* cntlr in hw.uhc) {
            // map controller ID to qemu compatible bus name
            NSString* bus = [NSString stringWithFormat:@"uhc%u", index++];
            busMap[cntlr.id] = bus;

            [params addObjectsFromArray:@[@"-device", [NSString stringWithFormat:@"%@,id=%@", cntlr.model, bus]]];
        }
    }

    // create USB devices (usb-host)
    for (HWUsb* usb in hw.usb) {
        if (nil == usb.locationId && nil == usb.vendorId && nil == usb.productId) {
            // ignore usb-host device without any match parameters specified
            continue;
        }

        [params addObject:@"-device"];

        NSMutableString* dev = [NSMutableString stringWithString:@"usb-host"];

        HWUhc* uhc = [hw providerOf:usb amongItems:hw.uhc];
        if (nil != uhc) {
            // use certain UHC, instead of default one (usb-bus)
            [dev appendFormat:@",bus=%@.0", busMap[uhc.id]];
        }

        if (usb.locationId)
            [dev appendFormat:@",hostaddr=0x%x", [usb.locationId unsignedIntValue]];
        if (usb.vendorId)
            [dev appendFormat:@",vendorid=0x%x", [usb.vendorId unsignedIntValue]];
        if (usb.productId)
            [dev appendFormat:@",productid=0x%x", [usb.productId unsignedIntValue]];

        [params addObject:dev];
    }

    return TRUE;
}

- (NSArray*) createLaunchVmParam: (NSString*) name withOptions: (VM *)vm restore: (bool) restore
{
    int i;
    char conf_name[256];
    NSMutableArray *params = [NSMutableArray array];

    // bios
    NSString *root = [[NSBundle mainBundle] bundlePath];
    NSString *bios_path;
    if ([root hasSuffix: @".app"])
        bios_path = [NSString stringWithFormat:@"%@/Contents/SharedSupport/bios", root];
    else
        bios_path = [NSString stringWithFormat:@"%@/bios", root];
    [params addObject: @"-L"];
    [params addObject: bios_path];
    // cpu
    VMHw *hw = vm.hw;
    HWCpu *cpu = hw.cpu;
    NSNumber *cores = cpu.cores;
    if (cores && [cores intValue] > 0) {
        [params addObject: @"-smp"];
        [params addObject: [NSString stringWithFormat: @"%d", [cores intValue]]];
    }
    // ram
    NSString *ram = hw.ram;
    if (ram) {
        [params addObject: @"-m"];
        [params addObject: ram];
    }

    // controller
    NSArray<HWDiskController *> *controllers = hw.disk_controller;
    for (i = 0; i < [controllers count]; i++)  {
        HWDiskController *controller = [controllers objectAtIndex: i];
        NSString *type = [controller.type lowercaseString];
        NSString *mode = [controller.mode lowercaseString];

        if ([type isEqualToString: @"ide"] && [mode isEqualToString: @"sata"]) {
            [params addObject: @"-device"];
            [params addObject: @"ich9-ahci,id=sata"];
        }

        if ([type isEqualToString: @"ide"])
            continue;

        NSString *model = DEFAULT_SCSI_MODEL;
        if (controller.model)
            model = controller.model;

        NSNumber *cid = controller.id;
        NSString *controller_id = [NSString stringWithFormat: @"%@%d", type, [cid intValue]];

        [params addObject: @"-device"];
        [params addObject: [NSString stringWithFormat: @"%@,id=%@", model, controller_id]];
    }

    // hd
    int sata_id = 0;
    NSArray<HWHd *> *hds = hw.hd;
    for (i = 0; i < [hds count]; i++)  {
        HWHd *hd = [hds objectAtIndex: i];
        NSNumber *disk_id = hd.id;

        NSNumber *cid = hd.controller;
        NSNumber *bus_id = hd.bus;

        HWDiskController *controller = [self findController: vm byId:[cid intValue]];
        if (!controller)
            continue;

        if (!bus_id) {
            bus_id = [NSNumber numberWithInt: [self generateBusId: vm controller: controller device: hd]];
            if ([bus_id integerValue] < 0)
                continue;
        }
        NSString *ctype = [controller.type lowercaseString];
        NSString *mode = [controller.mode lowercaseString];

        if ([ctype isEqualToString: @"scsi"]) {
            NSString *controller_id = [NSString stringWithFormat: @"%@%d.%d", ctype, [cid intValue], [bus_id intValue]];
            [params addObject: @"-device"];
            [params addObject:
             [NSString stringWithFormat: @"scsi-hd,bus=%@", controller_id]];
            snprintf(conf_name, 256, "disk.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        } else if ([mode isEqualToString: @"sata"]) {
            NSString *controller_id = [NSString stringWithFormat: @"sata.%d", sata_id++];
            [params addObject: @"-device"];
            [params addObject:
             [NSString stringWithFormat: @"ide-hd,bus=%@", controller_id]];
            snprintf(conf_name, 256, "disk.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        } else {
            // ide
            NSString *controller_id = [NSString stringWithFormat: @"%@.%d", ctype, [cid intValue]];
            [params addObject: @"-device"];
            [params addObject:
            [NSString stringWithFormat: @"ide-hd,bus=%@",
                 controller_id]];
            snprintf(conf_name, 256, "disk.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        }

        NSString *img = hd.file;
        if (img) {
            if ([[img lastPathComponent] isEqualToString: img])
                img = [[self getVmFolder: name] stringByAppendingPathComponent: img];
            [params addObject: @"-drive"];
            [params addObject: [NSString stringWithFormat: @"if=none,id=disk.%d,file=%@", [disk_id intValue], img]];
        }
    }

    // cd
    NSArray<HWCdrom *> *cds = hw.cdrom;
    for (i = 0; i < [cds count]; i++)  {
        HWCdrom *cd = [cds objectAtIndex: i];
        NSNumber *disk_id = cd.id;

        NSNumber *cid = cd.controller;
        NSNumber *bus_id = cd.bus;
        HWDiskController *controller = [self findController: vm byId: [cid intValue]];
        if (!controller)
            continue;

        if (!bus_id) {
            bus_id = [NSNumber numberWithInt: [self generateBusId: vm controller: controller device: cd]];
            if ([bus_id integerValue] < 0)
                continue;
        }

        NSString *ctype = [controller.type lowercaseString];
        NSString *mode = [controller.mode lowercaseString];
        NSString *cd_type = @"cd";
        if ([cd.type isEqualToString: @"image"])
            cd_type = @"drive";

        NSNumber *media_in = [cd.file length] > 0 ? [NSNumber numberWithBool:cd.media_in] : @(FALSE);
        if ([ctype isEqualToString: @"scsi"]) {
            NSString *controller_id = [NSString stringWithFormat: @"%@%d.%d", ctype, [cid intValue], [bus_id intValue]];
            [params addObject: @"-device"];
            [params addObject:
                [NSString stringWithFormat: @"scsi-%@,bus=%@", cd_type, controller_id]];
            snprintf(conf_name, 256, "cdrom.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        } else if ([mode isEqualToString: @"sata"]) {
            NSString *controller_id = [NSString stringWithFormat: @"sata.%d", sata_id++];
            [params addObject: @"-device"];
            [params addObject:
             [NSString stringWithFormat: @"ide-%@,bus=%@", cd_type, controller_id]];
            snprintf(conf_name, 256, "cdrom.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        } else {
            NSString *controller_id = [NSString stringWithFormat: @"%@.%d", ctype, [cid intValue]];
            [params addObject: @"-device"];
            [params addObject: [NSString stringWithFormat:
                @"ide-%@,bus=%@", cd_type, controller_id]];
            snprintf(conf_name, 256, "cdrom.%d", [disk_id intValue]);
            set_current_conf_name(conf_name);
        }
        cd_type = @"cdrom";
        if ([cd.type isEqualToString: @"image"])
            cd_type = @"disk";
        NSString *img = cd.file && 0 == access([cd.file fileSystemRepresentation], R_OK) ? cd.file : nil;

        [params addObject: @"-drive"];
        [params addObject: [NSString stringWithFormat: @"if=none,id=cdrom.%d,file=%@,media=%@",
                            [disk_id intValue], img ? img : @"", cd_type]];
    }

    NSArray<HWNic *> *nics = hw.nic;
    for (i = 0; i < [nics count]; i++)  {
        HWNic *nic = [nics objectAtIndex: i];
        NSString *model = nic.model;
        NSString *mac = nic.mac;
        NSString *conn = nic.connection;
        NSString *adv_params = @"";

        //if (nic.pci_bus)
          //  adv_params = [NSString stringWithFormat: @",bus=pci.%@", nic.pci_bus];
        if (nic.pci_addr) {
            if ([adv_params length])
                adv_params = [NSString stringWithFormat: @"%@,addr=%@", adv_params, nic.pci_addr];
            else
                adv_params = [NSString stringWithFormat: @",addr=%@", nic.pci_addr];
        }

        if (!model)
            model = @"e1000";
        if (nic.family)
            model = [NSString stringWithFormat: @"%@-%@", model, nic.family];

        [params addObject: @"-net"];
        if (mac)
            [params addObject: [NSString stringWithFormat: @"nic,vlan=%d,model=%@,macaddr=%@%@", i, model, mac, adv_params]];
        else
            [params addObject: [NSString stringWithFormat: @"nic,vlan=%d,model=%@%@", i, model, adv_params]];

        [params addObject: @"-net"];
        if ([conn isEqualToString: @"host"] || [conn isEqualToString: @"shared"]) {
            if (mac)
                [params addObject: [NSString stringWithFormat: @"vnet,uuid=%@,mode=%@", mac, conn]];
            else
                [params addObject: [NSString stringWithFormat: @"vnet,mode=%@", conn]];
        } else if ([conn isEqualToString: @"user"]) {
            [params addObject: @"user"];
        } else if ([conn isEqual:@"bridged"]) {
            if (nic.bridge)
                [params addObject: [NSString stringWithFormat:@"tap,bridge=%@,vlan=%d", nic.bridge, i]];
            else
                [params addObject: [NSString stringWithFormat:@"tap,vlan=%d", i]];
        } else {
            [params addObject: @"none"];
        }
    }

    // sound
    if ([hw.audio.type isEqualTo: @"usbaudio"]) {
        [params addObject: @"-usbdevice"];
        [params addObject: @"audio"];
    }

    // usb
    [self createUSBParam:hw parameters:params error:NULL];

    // misc
    NSString *boot_dev = @"d";
    if ([vm.general.boot_device isEqualToString: @"cd"])
        boot_dev = @"d";
    else
        boot_dev = @"c";
    [params addObject: @"-boot"];
    [params addObject: [NSString stringWithFormat: @"%@,menu=on,key=0x1,splash=%@/splash.jpg", boot_dev, bios_path]];

    if (vm.advanced.snapshot)
        [params addObject: @"-snapshot"];
    [params addObject: @"-localtime"];
    [params addObject: @"-enable-vmx"];
    if ([hw.acpi isEqualToString: @"off"])
        [params addObject: @"-no-acpi"];
    if (![hw.hpet isEqualToString: @"on"])
        [params addObject: @"-no-hpet"];
    if ([hw.hyperv isEqualToString: @"off"])
        [params addObject: @"-no-hyperv"];
    [params addObject: @"-vga"];
    if ([hw.vga isEqualToString: @"cirrus"])
        [params addObject: @"cirrus"];
    else if ([hw.vga isEqualToString: @"std"])
        [params addObject: @"std"];
    else
         [params addObject: @"vmware"];

    [params addObject: @"-qmp"];
    NSArray *cache = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    NSString *tmpFile = [[cache objectAtIndex:0] stringByAppendingPathComponent: vm.uuid];
    if ([tmpFile length] > 100)
        tmpFile = [tmpFile substringToIndex: 100]; // 108 is the maximum length of unix socket bind path
    [params addObject: [NSString stringWithFormat: @"unix:%@,server,nowait", tmpFile]];

    if ([[vm.general.os lowercaseString] isEqualToString: @"osx"]) {
        [params addObject: @"-bios"];
        [params addObject: @"osx_boot.img"];

        //[params addObject: @"-smbios"];
        //[params addObject: @"type=2"];

        [params addObject: @"-device"];
        [params addObject: @"isa-applesmc,osk=\"ourhardworkbythesewordsguardedpleasedontsteal(c)AppleComputerInc\""];

        [params addObject: @"-device"];
        [params addObject: @"usb-kbd"];
        [params addObject: @"-device"];
        [params addObject: @"usb-mouse"];
    }

    if (restore) {
        [params addObject: @"-loadvm2"];
        [params addObject: @"last_state"];
    }

    if (vm.advanced.headless)
        [params addObject: @"-nographic"];

    for (NSString *label in vm.advanced.port_forwardings) {
        VMPortForwarding *pf = vm.advanced.port_forwardings[label];
        [params addObject: @"-port_fwd"];
        [params addObject: [pf toQemuPortForwardingCmd]];
    }

    // no default devices
    [params addObject:@"-nodefaults"];

    return [NSArray arrayWithArray: params];
}

- (void) setAccessRightsToFiles: (NSString *)name
{
    VM *vm = [self readVmProperties: name];
    if (!vm)
        return;

    NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
    // cd
    NSArray<HWCdrom *> *cds = vm.hw.cdrom;
    for (HWCdrom *cd in cds)  {
        NSString *img = cd.file;
        if (img) {
            NSData *bookmark = [prefs objectForKey: img];
            if (bookmark) {
                BOOL isStale;
                NSError *error;
                NSURL* outUrl = [NSURL URLByResolvingBookmarkData: bookmark options:NSURLBookmarkResolutionWithSecurityScope
                                relativeToURL:nil bookmarkDataIsStale: &isStale error:&error];
                [outUrl stopAccessingSecurityScopedResource];
                [outUrl startAccessingSecurityScopedResource];
            }
        }
    }

    NSString *folder = vm.advanced.guest_tools.fs_folder;
    BOOL file_sharing = TRUE;
    if (!vm.advanced.guest_tools.file_sharing)
        file_sharing = FALSE;
    if (file_sharing && folder) {
        NSData *bookmark = [prefs objectForKey: folder];
        if (bookmark) {
            BOOL isStale;
            NSError *error;
            NSURL* outUrl = [NSURL URLByResolvingBookmarkData: bookmark options:NSURLBookmarkResolutionWithSecurityScope
                            relativeToURL:nil bookmarkDataIsStale: &isStale error:&error];
            [outUrl stopAccessingSecurityScopedResource];
            [outUrl startAccessingSecurityScopedResource];
        }
    }
}

- (pid_t)startVm: (NSString *) name
{
    VM *vm = [self readVmProperties: name];
    if (!vm)
        return -1;

    if (![self commitHds: name])
        return -1;

    [self setAccessRightsToFiles: name];

    NSURL *url = [[NSUserDefaults standardUserDefaults] stringForKey:@"vmx"] ?
        [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"vmx"]] :
        [[NSBundle mainBundle] URLForAuxiliaryExecutable: @"vmx.app"];
    NSBundle *vmx_bundle = [[NSBundle alloc] initWithURL: url];
    url = vmx_bundle.executableURL;

    NSString *vmPath = [self getVmFolder:name];

    NSArray *args = @[@"-vm", vmPath, @"-loadvm2"];
    @synchronized(vmstate_lock) {
        vmstate[name] = @(VMStateStarting);
        return [NSTask vlaunchTaskWithLaunchPath: [url path] arguments: args];
    }
}

- (pid_t) restartVm: (NSString *) name
{
    if ([self isVmRunning: name]) {
        if ([self sendVmCommand: name command: @"reset"])
            return -1;
    }

    VM *vm = [self readVmProperties: name];
    if (!vm)
        return -1;

    if (![self commitHds: name])
        return -1;

    [self setAccessRightsToFiles: name];

    NSURL *url = [[NSBundle mainBundle] URLForAuxiliaryExecutable: @"vmx.app"];
    NSBundle *vmx_bundle = [[NSBundle alloc] initWithURL: url];
    url = vmx_bundle.executableURL;

    NSArray *args = @[@"-vm", name];
    return [NSTask vlaunchTaskWithLaunchPath: [url path] arguments: args];
}

- (HWAudio *) createNewAudioDevice: (NSString *) type
{
    HWAudio *audio = [HWAudio new];
    audio.id = @0;
    audio.type = @"usbaudio";
    return audio;
}

- (HWNic *) createNewNic: (NSString *) vm_name
{
    NSString *model = DEFAULT_NIC_MODEL;
    NSString *connection = @"shared";
    int last_id = 0;

    if (vm_name) {
        VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];

        VMHw *hw = vm.hw;
        NSArray<HWNic *> *nics = hw.nic;
        if ([nics count]) {
            connection = @"none";
            model = nics[0].model;
        }

        for (HWNic *nic in nics) {
            if ([nic.id intValue] >= last_id)
                last_id = [nic.id intValue] + 1;
        }
    }

    HWNic *nic = [[HWNic alloc] init];
    nic.id = [NSNumber numberWithInt:last_id];
    nic.model = model;
    nic.connection = @"shared";
    uint8_t mac[6];
    char mac_str[80];
    generate_macaddr(mac);
    macaddr_to_string(mac, mac_str);
    nic.mac = [NSString stringWithUTF8String: mac_str];

    return nic;
}

- (void)generateMacAddresses: (NSString *) vm_name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;
    NSArray<HWNic *> *nics = hw.nic;
    bool changed = false;

    NSMutableArray<HWNic *> *new_nics = [NSMutableArray array];
    for (HWNic *n in nics) {
        if (!n.mac) {
            uint8_t mac[6];
            char mac_str[80];
            generate_macaddr(mac);
            macaddr_to_string(mac, mac_str);
            n.mac = [NSString stringWithUTF8String: mac_str];
            changed = true;
        }
        [new_nics addObject: n];
    }
    if (changed)
        [self setNics: vm_name nics: new_nics];
}

- (void) generateUUID: (NSString *) vm_name
{
    NSString *path = [self getVmPropertiesFileName:vm_name];
    NSMutableDictionary *vm = [NSMutableDictionary dictionaryWithContentsOfFile: path];
    if (vm) {
        [vm setObject: [[NSUUID UUID] UUIDString] forKey: @"uuid"];
        [vm writeToFile: path atomically: YES];
    }
}

- (int) countDevicesAttachedToController: (int) cid vm: (NSString *)vm_name
{
    int cnt = 0;

    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;
    NSArray<HWCdrom *> *cds = hw.cdrom;
    for (HWCdrom *cd in cds) {
        if ([cd.controller intValue] == cid)
            cnt++;
    }
    NSArray<HWHd *> *hds = hw.hd;
    for (HWHd *hd in hds) {
        if ([hd.controller intValue] == cid)
            cnt++;
    }
    return cnt;
}

- (int) countDevicesAttachedToIDE: (NSString *)vm_name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;
    int n = 0;

    NSArray<HWDiskController *> *controllers = hw.disk_controller;
    for (HWDiskController *c in controllers) {
        int cid = [c.id intValue];
        if ([c.type isEqualToString: @"ide"]) {
            n = [self countDevicesAttachedToController: cid vm: vm_name];
            break;
        }
    }
    return n;
}

- (HWCdrom *) createNewCdrom: (NSString *) vm_name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    int last_id = 0;
    int cid = -1;
    bool is_scsi = false;

    if (vm) {
        VMHw *hw = vm.hw;

        NSArray<HWCdrom *> *cds = hw.cdrom;
        for (HWCdrom *cd in cds) {
            if ([cd.id intValue] >= last_id) {
                last_id = [cd.id intValue];
            }
        }
        NSArray<HWDiskController *> *controllers = hw.disk_controller;
        for (HWDiskController *c in controllers) {
            cid = [c.id intValue];
            if ([c.type isEqualToString: @"scsi"]) {
                is_scsi = true;
                break;
            }
        }
        // ide, check how many devices already attached
        int n = [self countDevicesAttachedToIDE: vm_name];
        if (n >= 4 && !is_scsi)
            cid = -1;
    }
    if (-1 == cid)
        return nil;

    HWCdrom *cd = [[HWCdrom alloc] init];
    cd.id = [NSNumber numberWithInt:last_id+1];
    cd.controller = [NSNumber numberWithInt:cid];
    cd.file = @"";
    cd.media_in = NO;

    return cd;
}

- (HWHd *) createNewHd: (NSString *) vm_name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    int last_id = 0;
    int cid = -1;
    bool is_scsi = false;

    if (vm) {
        VMHw *hw = vm.hw;

        NSArray<HWHd *> *hds = hw.hd;
        for (HWHd *hd in hds) {
            if ([hd.id intValue] >= last_id) {
                last_id = [hd.id intValue];
            }
        }
        NSArray<HWDiskController *> *controllers = hw.disk_controller;
        for (HWDiskController *c in controllers) {
            cid = [c.id intValue];
            if ([c.type isEqualToString: @"scsi"]) {
                is_scsi = true;
                break;
            }
        }
        // ide, check how many devices already attached
        int n = [self countDevicesAttachedToIDE: vm_name];
        if (n >= 4 && !is_scsi)
            cid = -1;
    }
    if (-1 == cid)
        return nil;

    HWHd *hd = [[HWHd alloc] init];
    hd.id = [NSNumber numberWithInt:last_id+1];
    hd.controller = [NSNumber numberWithInt: cid];
    hd.file = [NSString  stringWithFormat: @"hd%d.img", last_id + 1];
    hd.size = @"20G";

    return hd;
}

- (BOOL) isHdCommited: (NSString *) vm_name hd: (int) hid
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;

    NSArray<HWHd *> *hds = hw.hd;
    for (HWHd *hd in hds) {
        if ([hd.id intValue] == hid) {
            NSString *path = [[self getVmFolder: vm_name] stringByAppendingPathComponent: hd.file];
            BOOL is_dir;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: path isDirectory: &is_dir];
            if (exists)
                return TRUE;
        }
    }
    return FALSE;
}

- (BOOL) commitHds: (NSString *) vm_name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;

    // hd
    NSArray<HWHd *> *hds = hw.hd;
    for (int i = 0; i < [hds count]; i++)  {
        HWHd *hd = hds[i];

        if (![self isHdCommited: vm_name hd: [hd.id intValue]]) {
            NSString *str_size = hd.size;
            NSString *img_path = [[self getVmFolder: vm_name]
                                  stringByAppendingPathComponent: hd.file];

            char *end;
            uint64_t size = strtosz_suffix([str_size UTF8String], &end, STRTOSZ_DEFSUFFIX_B);
            if (size <= 0) {
                NSLog(@"Invalid size: %@", str_size);
                NSAlert *alert = [[NSAlert alloc] init];
                [alert setMessageText:NSLocalizedString(@"FAILED_TO_LAUNCH_VM", nil)];
                [alert setInformativeText:NSLocalizedString(@"INVALID_DISK_SIZE", nil)];
                [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
                alert.alertStyle = NSWarningAlertStyle;
                [alert runModal];
                return FALSE;
            } else {

                if (!create_disk_image([img_path UTF8String], "qcow2", size)) {
                    NSLog(@"Failed to create a disk image");
                    NSAlert *alert = [[NSAlert alloc] init];
                    [alert setMessageText:NSLocalizedString(@"FAILED_TO_LAUNCH_VM", nil)];
                    [alert setInformativeText:NSLocalizedString(@"IMG_CREATE_FAILURE", nil)];
                    [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
                    alert.alertStyle = NSWarningAlertStyle;
                    [alert runModal];
                    return FALSE;
                }
                NSLog(@"Created a disk image of size %llu at path %@", size, img_path);
            }
        }
    }
    return TRUE;
}

- (void) deleteHdData: (NSString *) vm_name hd: (int) hid
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;

    // hd
    NSArray<HWHd *> *hds = hw.hd;
    for (HWHd *hd in hds) {
        if ([hd.id intValue] == hid) {
            NSString *path = [[self getVmFolder: vm_name] stringByAppendingPathComponent: hd.file];
            BOOL is_dir;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: path isDirectory: &is_dir];
            if (exists) {
                [[NSFileManager defaultManager]  removeItemAtPath: path error: nil];
            }
        }
    }
}

- (HWDiskController *) createNewSCSIController: (NSString *) vm_name
{
    int cid = 0;

    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];
    VMHw *hw = vm.hw;

    NSArray<HWDiskController *> *controllers = hw.disk_controller;
    for (HWDiskController *c in controllers) {
        if ([c.id intValue] > cid)
            cid = [c.id intValue];
    }

    HWDiskController *controller = [[HWDiskController alloc] init];
    controller.id = [NSNumber numberWithInt:cid+1];
    controller.type = @"scsi";
    controller.model = DEFAULT_SCSI_MODEL;

    return controller;
}

- (VM *) defaultVMConfig: (NSDictionary *) props
{
    VM *vm = [VM new];

    vm.version = @"1";
    vm.uuid = [[NSUUID UUID] UUIDString];
    VMHw *hw = [VMHw new];

    hw.cpu = [HWCpu new];
    if ([props objectForKey: @"cpu"])
        hw.cpu.cores = props[@"cpu"];
    else
        hw.cpu.cores = @1;

    if ([props objectForKey: @"ram"])
        hw.ram = props[@"ram"];
    else
        hw.ram = @"1G";
    hw.chipset = @"default";

    NSMutableArray<HWDiskController *> *controllers = [NSMutableArray array];
    HWDiskController *controller = [HWDiskController new];
    controller.id = @0;
    controller.type = @"ide";

    if (props[@"ideMode"])
        controller.mode = props[@"ideMode"];

    [controllers addObject: controller];

    if ([props objectForKey: @"useSCSI"]) {
        HWDiskController *controller = [HWDiskController new];
        controller.id = @1;
        controller.type = @"scsi";
        controller.model = DEFAULT_SCSI_MODEL;
        [controllers addObject: controller];
    }
    hw.disk_controller = controllers;

    NSNumber *c_id = @0;
    if ([props objectForKey: @"useSCSI"])
        c_id = @1;

    NSMutableArray<HWHd *> *hds = [NSMutableArray array];
    HWHd *hd = [HWHd new];
    hd.id = @0;
    hd.controller = c_id;
    hd.file = @"hd0.img";
    if ([props objectForKey: @"hdSize"])
        hd.size = props[@"hdSize"];
    else
        hd.size = @"20G";
    hd.boot = [NSNumber numberWithBool:YES];
    [hds addObject: hd];
    hw.hd = hds;

    NSMutableArray<HWCdrom *> *cdroms = [NSMutableArray array];
    HWCdrom *cd = [HWCdrom new];
    cd.id = @0;
    cd.controller = @0;
    if ([props objectForKey: @"iso"]) {
        cd.file = props[@"iso"];
        cd.media_in = YES;
    }
    [cdroms addObject: cd];
    hw.cdrom = cdroms;

    hw.audio = [self createNewAudioDevice: nil];

    NSMutableArray<HWNic *> *nics = [NSMutableArray array];
    HWNic *nic = [self createNewNic: nil];
    if ([props objectForKey: @"nic"])
        nic.model = props[@"nic"];
    if ([props objectForKey: @"nic_pci_addr"])
        nic.pci_addr = props[@"nic_pci_addr"];
    if ([props objectForKey: @"nic_family"])
        nic.family = props[@"nic_family"];
    [nics addObject: nic];
    hw.nic = nics;

    vm.hw = hw;

    VMGeneral *gen = [VMGeneral new];
    gen.os = props[@"os"];
    gen.os_family = props[@"os"];
    vm.general = gen;

    return vm;
}

- (VM *) defaultVMConfigSCSI: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: [NSNumber numberWithBool: TRUE] forKey: @"useSCSI"];
    [new_props setObject: @"sata" forKey: @"ideMode"];
    [new_props setObject: @"2G" forKey: @"ram"];
    return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigOSX: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: @"sata" forKey: @"ideMode"];
    [new_props setObject: @"30G" forKey: @"hdSize"];
    [new_props setObject: [NSNumber numberWithInt: 1] forKey: @"cpu"];
    [new_props setObject: @"2G" forKey: @"ram"];
    [new_props setObject: @"e1000" forKey: @"nic"];
    [new_props setObject: @"82545em" forKey: @"nic_family"];
    [new_props setObject: @"5"  forKey: @"nic_pci_addr"];
    [new_props setObject: @"osx"  forKey: @"os"];

    return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigWin: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: @"sata" forKey: @"ideMode"];
    [new_props setObject: [NSNumber numberWithBool: TRUE] forKey: @"useSCSI"];
    [new_props setObject: @"30G" forKey: @"hdSize"];
    [new_props setObject: [NSNumber numberWithInt: 2] forKey: @"cpu"];
    [new_props setObject: @"2G" forKey: @"ram"];

    return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigWinXp: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: @"rtl8139" forKey: @"nic"];
    return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigWin7: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: @"30G" forKey: @"hdSize"];
    [new_props setObject: [NSNumber numberWithInt: 2] forKey: @"cpu"];
    [new_props setObject: @"2G" forKey: @"ram"];

     return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigWin71: (NSDictionary *) props
{
    NSMutableDictionary *new_props = [NSMutableDictionary dictionaryWithDictionary: props];
    [new_props setObject: @"30G" forKey: @"hdSize"];
    [new_props setObject: [NSNumber numberWithInt: 2] forKey: @"cpu"];
    [new_props setObject: @"sata" forKey: @"ideMode"];
    [new_props setObject: @"2G" forKey: @"ram"];

    return [self defaultVMConfig: new_props];
}

- (VM *) defaultVMConfigIDE: (NSDictionary *) props
{
    return [self defaultVMConfig: props];
}

- (void) importTemporaryVM: (NSString *) name fromFolder: (NSString*) folder
{
    self.temp_vm_name = name;
    self.temp_vm_folder = folder;
}

- (void) addTemporaryVM: (NSDictionary*) props
{
    NSString *name = [props objectForKey: @"name"];
    //NSString *iso = [props objectForKey: @"iso"];
    NSString *os = [props objectForKey: @"os"];
    NSString *osFamily = [props objectForKey: @"os_family"];

    // build a temporary vm settings
    self.temp_vm_name = name;
    SEL vm_create = nil;
    NSArray* oses = [self.os_list objectForKey: os];
    for (NSDictionary *d in oses) {
        if ([[d objectForKey: @"name"] isEqualTo: osFamily]) {
            vm_create = NSSelectorFromString([d objectForKey: @"cfg"]);
            break;
        }
    }
    VM *vm = [self performSelector: vm_create withObject: props];

    NSURL *directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:    [[NSProcessInfo processInfo] globallyUniqueString]] isDirectory:YES];
    [[NSFileManager defaultManager] createDirectoryAtURL:directoryURL withIntermediateDirectories:YES attributes:nil error: nil];
    self.temp_vm_folder = [directoryURL path];

    NSString *plist_file = [self.temp_vm_folder stringByAppendingPathComponent: @"settings.plist"];
    if (![[vm toDictionary] writeToFile: plist_file atomically: YES])
        [NSException raise:@"Cannot create properties file" format:@"file %@", plist_file];

//    self.temp_vm = vm;
}

- (void) cleanupTempVm
{
    if ([[NSFileManager defaultManager] fileExistsAtPath: self.temp_vm_folder isDirectory: nil])
          [[NSFileManager defaultManager] removeItemAtPath: self.temp_vm_folder error: nil];
    self.temp_vm_name = nil;
    self.temp_vm_folder = @"";
}

- (BOOL) commitTemporaryVM
{
    NSString *path = [self.temp_vm_folder stringByAppendingPathComponent: @"settings.plist"];
    VM *vm = [VM fromDictionary:[NSDictionary dictionaryWithContentsOfFile: path]];

    path = [self.vmLibPath stringByAppendingPathComponent: self.temp_vm_name];

    VMHw *hw = vm.hw;
    NSArray<HWHd *> *hds = hw.hd;

    uint64_t total_size = 0;
    for (HWHd *hd in hds) {
        char *end;

        NSString *str_size = hd.size;
        NSString *hd_path = [path stringByAppendingPathComponent: hd.file];
        BOOL is_dir;
        BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: hd_path isDirectory: &is_dir];
        if (str_size && !exists) {
            uint64_t size = strtosz_suffix([str_size UTF8String], &end, STRTOSZ_DEFSUFFIX_B);
            if (size > 0) {
                total_size += size;
            }
        }
    }

    if (total_size > [SystemInfo freeDiskspace]) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"NOT_ENOUGH_SPACE", nil)];
        [alert setInformativeText:NSLocalizedString(@"NOT_ENOUGH_SPACE_MSG", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Continue", nil)];
        alert.alertStyle = NSWarningAlertStyle;
        if ([alert runModal] == NSAlertFirstButtonReturn)
            return FALSE;
    }

    BOOL res = [[NSFileManager defaultManager] moveItemAtPath: self.temp_vm_folder toPath: path error: nil];
    if (!res) {
        NSLog(@"Failed to create VM folder at %@", path);
    }

    [self cleanupTempVm];
    return res;
}

- (BOOL) isTemporaryVM: (NSString*) vm_name
{
    return [self.temp_vm_name isEqualTo: vm_name];
}

- (NSString*) getVmFolder: (NSString*) vm_name
{
    BOOL dir = FALSE;
    if ([[NSFileManager new] fileExistsAtPath:vm_name isDirectory:&dir] && dir)
        return [NSString stringWithString:vm_name];

    if ([self isTemporaryVM: vm_name])
        return self.temp_vm_folder;

    assert(self.vmLibPath);
    return [self.vmLibPath stringByAppendingPathComponent: vm_name];
}


- (NSString *) sendVmCommand: (NSString *) vm_name command: (NSString *)cmd
{
    int sock;
    struct sockaddr_un remote;

    if ([vm_name isEqualTo: self.temp_vm_name])
        return nil;

    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vm_name];

    if (!vm)
        return nil;

    NSString *uuid = vm.uuid;
    NSArray *cache = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    NSString *tmpFile = [[cache objectAtIndex: 0] stringByAppendingPathComponent: uuid];
    if ([tmpFile length] > 100)
        tmpFile = [tmpFile substringToIndex: 100]; // 108 is the maximum length of unix socket bind path

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return nil;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500 ms

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, [tmpFile UTF8String], sizeof(remote.sun_path));
    if (connect(sock, (struct sockaddr *)&remote, (socklen_t)sizeof(remote)) == -1) {
        close(sock);
        return nil;
    }

    // add \n
    if ([cmd characterAtIndex: [cmd length] - 1] != '\n')
        cmd = [NSString stringWithFormat: @"%@\n", cmd];
    if (send(sock, [cmd UTF8String], [cmd length], 0) == -1) {
        close(sock);
        return nil;
    }
    fsync(sock);

    char response[256];
    memset(response, 0, sizeof(response));

    if (readLine(sock, response, sizeof(response)) < 0) {
        close(sock);
        return nil;
    }

    close(sock);
    return [NSString stringWithUTF8String: response];
}

- (VMState) getVmState: (NSString *) name
{
    VMState state = VMStateStopped;
    VMState prevState = VMStateStopped;
    @synchronized(vmstate_lock) {
        if (vmstate[name])
            prevState = [vmstate[name] unsignedLongValue];
    }

    if ([self isVmRunning: name])
        state = VMStateRunning;
    else if ([self isVmSuspending: name]) {
        if (prevState != VMStateStarting)
            state = VMStateSuspending;
        else
            state = VMStateStarting;
    }
    else if ([self isVmSuspended: name])
        state = VMStateSuspended;
    @synchronized(vmstate_lock) {
        vmstate[name] = [NSNumber numberWithUnsignedLong:state];
    }
    return state;
}

- (NSString*) getIpAddress: (NSString *) name
{
    if ([name isEqualTo: self.temp_vm_name])
        return nil;
    NSString *resp = [self sendVmCommand: name command: @"ip_addr"];
    if (!resp)
        return nil;
    return [resp stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
}

- (bool) isVmRunning: (NSString *) name
{
    if ([name isEqualTo: self.temp_vm_name])
        return false;
    NSString *resp = [self sendVmCommand: name command: @"status"];
    if (!resp)
        return false;
    return true;
}

- (bool) isVmSuspending: (NSString *) name
{
    if ([name isEqualTo: self.temp_vm_name])
        return false;

    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: name];
    if (!vm)
        return false;
    VMHw *hw = vm.hw;
    NSArray<HWHd *> *hds = hw.hd;

    for (HWHd *hd in hds) {
        NSString *img = hd.file;
        if (img) {
            if ([[img lastPathComponent] isEqualToString: img])
                img = [[self getVmFolder: name] stringByAppendingPathComponent: img];

            BOOL is_dir;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: img isDirectory: &is_dir];

            if (exists) {
                //NSDictionary *attributes =  [[NSFileManager defaultManager] attributesOfItemAtPath: img error: nil];
                int h = open([img UTF8String], O_RDWR | O_NONBLOCK | O_EXLOCK);
                if (h >= 0) {
                    /*struct flock flock;
                    if (fcntl(h, F_GETLK, &flock) >= 0) {
                        // Possibly print out other members of flock as well...
                        NSLog(@"l_type=%d\n", (int)flock.l_type);
                        //return true;
                    }*/
                    close(h);
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}

- (bool) isVmSuspended: (NSString *) name
{
    if ([name isEqualTo: self.temp_vm_name])
        return false;

    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: name];
    if (!vm)
        return false;
    VMHw *hw = vm.hw;
    NSArray<HWHd *> *hds = hw.hd;

    for (HWHd *hd in hds) {
        NSString *img = hd.file;
        if (img) {
            if ([[img lastPathComponent] isEqualToString: img])
                img = [[self getVmFolder: name] stringByAppendingPathComponent: img];

            BOOL is_dir;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: img isDirectory: &is_dir];

            if (exists && find_snapshot([img UTF8String], "last_state"))
                return true;
        }
    }
    return false;
}

- (void) shutoffVm: (NSString *) name
{
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: name];
    if (!vm)
        return;

    if ([self isVmRunning: name]) {
        [self sendVmCommand: name command: @"shutoff"];
    }

    VMHw *hw = vm.hw;
    NSArray<HWHd *> *hds = hw.hd;

    for (HWHd *hd in hds) {
        NSString *img = hd.file;
        if (img) {
            if ([[img lastPathComponent] isEqualToString: img])
                img = [[self getVmFolder: name] stringByAppendingPathComponent: img];

            delete_snapshot([img UTF8String], "last_state");
        }
    }
    @synchronized(vmstate_lock) {
        vmstate[name] = [NSNumber numberWithUnsignedLong:VMStateStopped];
    }
}

- (bool) shutdownVm: (NSString *) name
{
    if ([self isVmRunning: name]) {
        [self sendVmCommand: name command: @"shutdown"];
        return true;
    }
    return false;
}

- (bool) suspendVm: (NSString *) name
{
    if ([self isVmRunning: name]) {
        [self sendVmCommand: name command: @"suspend"];
        @synchronized(vmstate_lock) {
            vmstate[name] = [NSNumber numberWithUnsignedLong:VMStateSuspending];
        }
        return true;
    }
    return false;
}

- (bool) rebootVm: (NSString *) name
{
    if ([self isVmRunning: name]) {
        [self sendVmCommand: name command: @"reboot"];
        return true;
    }
    return false;
}

- (int) maxSCSIControllers
{
    return 1;
}

- (int) maxNICs
{
    return 4;
}

- (int) maxHDs
{
    return 4;
}

- (int) maxCDROMs
{
    return 4;
}

- (int) maxUHCs
{
    return 4;
}

- (NSString*) makeLegalVmName: (NSString*) name
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *new_path = [NSString stringWithFormat: @"%@/%@", self.vmLibPath, name];
    NSString *free_new_name = [NSString stringWithString: name];

    int counter = 1;
    while ([fm fileExistsAtPath: new_path] && counter < 256) {
        free_new_name = [NSString stringWithFormat:@"%@(%d)", name, counter];
        new_path = [NSString stringWithFormat: @"%@/%@", self.vmLibPath, free_new_name];
        counter++;
    }
    return free_new_name;
}

- (NSString*) renameVM: (NSString*) old_name to: new_name
{
    [self setVmDisplayName: old_name display: new_name];
    return new_name;
}

- (BOOL) deleteVm: (NSString*) vm_name
{
    NSString *path = [NSString stringWithFormat: @"%@/%@", self.vmLibPath, vm_name];
    return [[NSFileManager defaultManager] removeItemAtPath: path error: nil];

}

- (void) setVmSortOrder: (NSArray*) sorted_vm_list
{
    NSMutableArray *d = [NSMutableArray array];

    for (NSString *vm_name in sorted_vm_list)
        [d addObject: vm_name];

    NSString *path = [NSString stringWithFormat: @"%@/%@", self.vmLibPath, @".sortkey"];
    [d writeToFile: path atomically: NO];
}

- (void) setVmLibraryFolder: (NSString *) vmlib_path
{
    BOOL is_dir;
    if (![[vmlib_path lastPathComponent] isEqualToString: VM_LIB_DIR])
        vmlib_path = [vmlib_path stringByAppendingPathComponent: VM_LIB_DIR];
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath: vmlib_path isDirectory: &is_dir] && is_dir;

    if (!exists) {
        NSError *error = nil;
        if ([[NSFileManager defaultManager] createDirectoryAtPath: vmlib_path withIntermediateDirectories: TRUE attributes: nil error: &error])
            exists = TRUE;
        else {
            NSLog(@"Failed to create dir: %@", error);
            NSAlert *alert = [NSAlert alertWithError: error];
            [alert runModal];
            return;
        }
    }
    self.vmLibPath = vmlib_path;

    NSData* data = [[NSURL fileURLWithPath: vmlib_path] bookmarkDataWithOptions:
                    NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
    if (data)
        [prefs setObject:data forKey: vmlib_path];
    [prefs setObject: vmlib_path forKey: @"VMLIB_PATH"];
    [prefs synchronize];
}

@end

NSString *RealHomeDirectory()
{
    return NSHomeDirectory();
}


NSString *RealDownloadsDirectory(){
    return [RealHomeDirectory() stringByAppendingPathComponent: @"Downloads"];
}
