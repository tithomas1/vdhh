#include "qsysbus.h"

// call this function if you need these (below) static initializers in your module
void img_init() {

}

void bdrv_blkdebug_init(void);
void bdrv_bochs_init(void);
void bdrv_cloop_init(void);
void bdrv_dmg_init(void);
void bdrv_parallels_init(void);
void bdrv_qcow_init(void);
void bdrv_qcow2_init(void);
void bdrv_qed_init(void);
void bdrv_file_init(void);
void bdrv_raw_init(void);
//void bdrv_vhdx_init(void);
void bdrv_vmdk_init(void);
void bdrv_vpc_init(void);
void bdrv_vvfat_init(void);

block_init(bdrv_vvfat_init);
block_init(bdrv_vpc_init);
block_init(bdrv_vmdk_init);
//block_init(bdrv_vhdx_init);
block_init(bdrv_raw_init);
block_init(bdrv_file_init)
block_init(bdrv_qed_init);
block_init(bdrv_qcow2_init);
block_init(bdrv_qcow_init);
block_init(bdrv_parallels_init);
block_init(bdrv_dmg_init);
block_init(bdrv_cloop_init);
block_init(bdrv_bochs_init);
block_init(bdrv_blkdebug_init);
