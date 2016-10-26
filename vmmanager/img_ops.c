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

#include "img_ops.h"
#include "block_int.h"
#include "blockjob.h"
#include "emublock-backend.h"

BlockBackend *blk_new_with_bs(const char *name, Error **errp);
BlockDriverState *blk_bs(BlockBackend *blk);
int bdrv_snapshot_list(BlockDriverState *bs, QEMUSnapshotInfo **psn_info);

void debug(const char *message,...);

static bool first_time = true;

void img_ops_init()
{
    if (first_time) {
        first_time = false;

        bdrv_init();

        Error *local_error = NULL;
        if (vmx_process_events_init(&local_error)) {
            error_report("%s", error_get_pretty(local_error));
            error_free(local_error);
            exit(EXIT_FAILURE);
        }
    }
}

static BlockDriverState *img_open(const char *id1, const char *filename,
                              const char *fmt, int flags,
                              bool require_io, bool quiet)
{
    BlockBackend *blk;
    BlockDriverState *bs;
    BlockDriver *drv;
    Error *local_err = NULL;
    int ret;

    img_ops_init();

    blk = blk_new_with_bs(id1, &error_abort);
    bs = blk_bs(blk);

    if (fmt) {
        drv = bdrv_find_format(fmt);
        if (!drv) {
            error_report("Unknown file format '%s'", fmt);
            goto fail;
        }
    } else {
        drv = NULL;
    }

    ret = bdrv_open(&bs, filename, NULL, NULL, flags, drv, &local_err);
    if (ret < 0) {
        error_report("Could not open '%s': %s", filename,
                     error_get_pretty(local_err));
        error_free(local_err);
        goto fail;
    }

    if (bdrv_is_encrypted(bs) && require_io) {
        debug("Disk image '%s' is encrypted", filename);
        goto fail;
    }
    return bs;
fail:
    blk_unref(blk);
    return NULL;
}

uint64_t get_vm_image_size(const char *filename)
{
    int bdrv_oflags = BDRV_O_CACHE_WB | BDRV_O_RDWR;
    BlockDriverState *bs = img_open("image", filename, NULL, bdrv_oflags, true, false);
    if (!bs)
        return 0;
    BlockBackend *blk = bs->blk;

    uint64_t len = blk_getlength(blk);

    blk_unref(bs->blk);
    return len;
}

bool create_disk_image(const char* path, const char *fmt, uint64_t img_size)
{
    Error *local_err = NULL;

    img_ops_init();

    bdrv_img_create(path, fmt, NULL, NULL, NULL, img_size, BDRV_O_CACHE_WB, &local_err, true);
    if (local_err) {
        debug("%s: %s", path, error_get_pretty(local_err));
        error_free(local_err);
        return false;
    }
    return true;
}

bool find_snapshot(const char* path, const char *snapshot_name)
{
    //Error *local_err = NULL;
    bool ret = false;
    img_ops_init();

    int bdrv_oflags = BDRV_O_CACHE_WB;
    BlockDriverState *bs = img_open("image", path, NULL, bdrv_oflags, false, true);
    if (!bs)
        return false;

    QEMUSnapshotInfo *sn_tab, *sn;
    int nb_sns, i;

    nb_sns = bdrv_snapshot_list(bs, &sn_tab);
    if (nb_sns <= 0)
        goto exit2;

    if (!strcmp(sn_tab->name, snapshot_name)) {
        ret = true;
        goto exit;
    }

    for(i = 0; i < nb_sns; i++) {
        sn = &sn_tab[i];
        if (!strcmp(sn->name, snapshot_name)) {
            ret = true;
            goto exit;
        }
    }

exit:
    g_free(sn_tab);
exit2:
    blk_unref(bs->blk);
    return ret;
}

bool delete_snapshot(const char* path, const char *snapshot_name)
{
    Error *local_err = NULL;
    bool ret = false;
    img_ops_init();
    
    int bdrv_oflags = BDRV_O_CACHE_WB | BDRV_O_RDWR;
    BlockDriverState *bs = img_open("image", path, NULL, bdrv_oflags, false, true);
    if (!bs)
        return false;

    ret = (bdrv_snapshot_delete(bs, NULL, snapshot_name, &local_err) == 0);

    blk_unref(bs->blk);
    return ret;
}
