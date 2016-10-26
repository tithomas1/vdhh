/*
 * QEMU MegaRAID SAS 8708EM2 Host Bus Adapter emulation
 * Based on the linux driver code at drivers/scsi/megaraid
 *
 * Copyright (c) 2009-2012 Hannes Reinecke, SUSE Labs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hw.h"
#include "pci.h"
#include "emudma.h"
#include "emublock-backend.h"
#include "qemu/iov.h"
#include "nscsi.h"
#include <scsi.h>

#include "mfi.h"

#define DPRINTF {}

#define MEGASAS_VERSION_GEN1 "1.70"
#define MEGASAS_VERSION_GEN2 "1.80"
#define MEGASAS_MAX_FRAMES 2048         /* Firmware limit at 65535 */
#define MEGASAS_DEFAULT_FRAMES  1000     /* Windows requires this */
#define MEGASAS_GEN2_DEFAULT_FRAMES 1008     /* Windows requires this */
#define MEGASAS_MAX_SGE 128             /* Firmware limit */
#define MEGASAS_DEFAULT_SGE 80
#define MEGASAS_MAX_SECTORS 0xFFFF      /* No real limit */
#define MEGASAS_MAX_ARRAYS 128

#define MEGASAS_HBA_SERIAL "VEERTU1234"
#define NAA_LOCALLY_ASSIGNED_ID 0x3ULL
#define IEEE_COMPANY_LOCALLY_ASSIGNED 0x525400

#define MEGASAS_FLAG_USE_JBOD      0
#define MEGASAS_MASK_USE_JBOD      (1 << MEGASAS_FLAG_USE_JBOD)
#define MEGASAS_FLAG_USE_MSI       1
#define MEGASAS_MASK_USE_MSI       (1 << MEGASAS_FLAG_USE_MSI)
#define MEGASAS_FLAG_USE_MSIX      2
#define MEGASAS_MASK_USE_MSIX      (1 << MEGASAS_FLAG_USE_MSIX)
#define MEGASAS_FLAG_USE_QUEUE64   3
#define MEGASAS_MASK_USE_QUEUE64   (1 << MEGASAS_FLAG_USE_QUEUE64)

static const char *mfi_frame_desc[] = {
    "MFI init", "LD Read", "LD Write", "LD SCSI", "PD SCSI",
    "MFI Doorbell", "MFI Abort", "MFI SMP", "MFI Stop"};

typedef struct MegasasCmd {
    uint32_t index;
    uint16_t flags;
    uint16_t count;
    uint64_t context;

    hwaddr pa;
    hwaddr pa_size;
    union mfi_frame *frame;
    SCSIRequest *req;
    VeertuSGList qsg;
    void *iov_buf;
    size_t iov_size;
    size_t iov_offset;
    struct MegasasState *state;
} MegasasCmd;

typedef struct MegasasState {
    /*< private >*/
    PCIDevice parent;
    /*< public >*/

    VeertuMemArea mmio_io;
    VeertuMemArea port_io;
    VeertuMemArea queue_io;
    uint32_t frame_hi;

    int fw_state;
    uint32_t fw_sge;
    uint32_t fw_cmds;
    uint32_t flags;
    int fw_luns;
    int intr_mask;
    int doorbell;
    int busy;
    int diag;
    int adp_reset;

    MegasasCmd *event_cmd;
    int event_locale;
    int event_class;
    int event_count;
    int shutdown_event;
    int boot_event;

    uint64_t sas_addr;
    char *hba_serial;

    uint64_t reply_queue_pa;
    //void *reply_queue;
    int reply_queue_len;
    int reply_queue_head;
    int reply_queue_tail;
    uint64_t consumer_pa;
    uint64_t producer_pa;

    MegasasCmd frames[MEGASAS_MAX_FRAMES];
    DECLARE_BITMAP(frame_map, MEGASAS_MAX_FRAMES);
    SCSIBus bus;
} MegasasState;

typedef struct MegasasBaseClass {
    PCIDeviceClass parent_class;
    const char *product_name;
    const char *product_version;
    int mmio_bar;
    int ioport_bar;
    int osts;
} MegasasBaseClass;

#define TYPE_MEGASAS_BASE "megasas-base"
#define TYPE_MEGASAS_GEN1 "megasas"
#define TYPE_MEGASAS_GEN2 "megasas-gen2"

#define MEGASAS(obj) obj
#define MEGASAS_DEVICE_CLASS(oc) oc
#define MEGASAS_DEVICE_GET_CLASS(oc) ((VeertuType *)oc)->class

#define MEGASAS_INTR_DISABLED_MASK 0xFFFFFFFF

static bool megasas_intr_enabled(MegasasState *s)
{
    if ((s->intr_mask & MEGASAS_INTR_DISABLED_MASK) !=
        MEGASAS_INTR_DISABLED_MASK) {
        return true;
    }
    return false;
}

static bool megasas_use_queue64(MegasasState *s)
{
    return s->flags & MEGASAS_MASK_USE_QUEUE64;
}

static bool megasas_use_msi(MegasasState *s)
{
    /*return s->flags & MEGASAS_MASK_USE_MSI;*/
    return 0;
}

static bool megasas_use_msix(MegasasState *s)
{
    //return s->flags & MEGASAS_MASK_USE_MSIX;
    return 0;
}

static bool megasas_is_jbod(MegasasState *s)
{
    return s->flags & MEGASAS_MASK_USE_JBOD;
}

static void megasas_frame_set_cmd_status(unsigned long frame, uint8_t v)
{
    stb_phys(&address_space_memory,
             frame + offsetof(struct mfi_frame_header, cmd_status), v);
}

static void megasas_frame_set_scsi_status(unsigned long frame, uint8_t v)
{
    stb_phys(&address_space_memory,
             frame + offsetof(struct mfi_frame_header, scsi_status), v);
}

/*
 * Context is considered opaque, but the HBA firmware is running
 * in little endian mode. So convert it to little endian, too.
 */
static uint64_t megasas_frame_get_context(unsigned long frame)
{
    return ldq_le_phys(&address_space_memory,
                       frame + offsetof(struct mfi_frame_header, context));
}

static bool megasas_frame_is_ieee_sgl(MegasasCmd *cmd)
{
    return cmd->flags & MFI_FRAME_IEEE_SGL;
}

static bool megasas_frame_is_sgl64(MegasasCmd *cmd)
{
    return cmd->flags & MFI_FRAME_SGL64;
}

static bool megasas_frame_is_sense64(MegasasCmd *cmd)
{
    return cmd->flags & MFI_FRAME_SENSE64;
}

static uint64_t megasas_sgl_get_addr(MegasasCmd *cmd,
                                     union mfi_sgl *sgl)
{
    uint64_t addr;

    if (megasas_frame_is_ieee_sgl(cmd)) {
        addr = le64_to_cpu(sgl->sg_skinny->addr);
    } else if (megasas_frame_is_sgl64(cmd)) {
        addr = le64_to_cpu(sgl->sg64->addr);
    } else {
        addr = le32_to_cpu(sgl->sg32->addr);
    }
    return addr;
}

static uint32_t megasas_sgl_get_len(MegasasCmd *cmd,
                                    union mfi_sgl *sgl)
{
    uint32_t len;

    if (megasas_frame_is_ieee_sgl(cmd)) {
        len = le32_to_cpu(sgl->sg_skinny->len);
    } else if (megasas_frame_is_sgl64(cmd)) {
        len = le32_to_cpu(sgl->sg64->len);
    } else {
        len = le32_to_cpu(sgl->sg32->len);
    }
    return len;
}

static union mfi_sgl *megasas_sgl_next(MegasasCmd *cmd,
                                       union mfi_sgl *sgl)
{
    uint8_t *next = (uint8_t *)sgl;

    if (megasas_frame_is_ieee_sgl(cmd)) {
        next += sizeof(struct mfi_sg_skinny);
    } else if (megasas_frame_is_sgl64(cmd)) {
        next += sizeof(struct mfi_sg64);
    } else {
        next += sizeof(struct mfi_sg32);
    }

    if (next >= (uint8_t *)cmd->frame + cmd->pa_size) {
        return NULL;
    }
    return (union mfi_sgl *)next;
}

static void megasas_soft_reset(MegasasState *s);

static int megasas_map_sgl(MegasasState *s, MegasasCmd *cmd, union mfi_sgl *sgl)
{
    int i;
    int iov_count = 0;
    size_t iov_size = 0;

    cmd->flags = le16_to_cpu(cmd->frame->header.flags);
    iov_count = cmd->frame->header.sge_count;
    if (iov_count > MEGASAS_MAX_SGE) {
        return iov_count;
    }
    pci_dma_sglist_init(&cmd->qsg, PCI_DEVICE(s), iov_count);
    for (i = 0; i < iov_count; i++) {
        uint64_t iov_pa, iov_size_p;

        if (!sgl) {
            goto unmap;
        }
        iov_pa = megasas_sgl_get_addr(cmd, sgl);
        iov_size_p = megasas_sgl_get_len(cmd, sgl);
        if (!iov_pa || !iov_size_p) {
            goto unmap;
        }
        veertu_sglist_add(&cmd->qsg, iov_pa, iov_size_p);
        sgl = megasas_sgl_next(cmd, sgl);
        iov_size += (size_t)iov_size_p;
    }
    cmd->iov_offset = 0;
    return 0;
unmap:
    veertu_sglist_destroy(&cmd->qsg);
    return iov_count - i;
}

static void megasas_unmap_sgl(MegasasCmd *cmd)
{
    veertu_sglist_destroy(&cmd->qsg);
    cmd->iov_offset = 0;
}

/*
 * passthrough sense and io sense are at the same offset
 */
static int megasas_build_sense(MegasasCmd *cmd, uint8_t *sense_ptr,
    uint8_t sense_len)
{
    PCIDevice *pcid = PCI_DEVICE(cmd->state);
    uint32_t pa_hi = 0, pa_lo;
    hwaddr pa;

    if (sense_len > cmd->frame->header.sense_len) {
        sense_len = cmd->frame->header.sense_len;
    }
    if (sense_len) {
        pa_lo = le32_to_cpu(cmd->frame->pass.sense_addr_lo);
        if (megasas_frame_is_sense64(cmd)) {
            pa_hi = le32_to_cpu(cmd->frame->pass.sense_addr_hi);
        }
        pa = ((uint64_t) pa_hi << 32) | pa_lo;
        pci_dma_write(pcid, pa, sense_ptr, sense_len);
        cmd->frame->header.sense_len = sense_len;
    }
    return sense_len;
}

static void megasas_write_sense(MegasasCmd *cmd, SCSISense sense)
{
    uint8_t sense_buf[SCSI_SENSE_BUF_SIZE];
    uint8_t sense_len = 18;

    memset(sense_buf, 0, sense_len);
    sense_buf[0] = 0xf0;
    sense_buf[2] = sense.key;
    sense_buf[7] = 10;
    sense_buf[12] = sense.asc;
    sense_buf[13] = sense.ascq;
    megasas_build_sense(cmd, sense_buf, sense_len);
}

static void megasas_copy_sense(MegasasCmd *cmd)
{
    uint8_t sense_buf[SCSI_SENSE_BUF_SIZE];
    uint8_t sense_len;

    sense_len = scsi_req_get_sense(cmd->req, sense_buf,
                                   SCSI_SENSE_BUF_SIZE);
    megasas_build_sense(cmd, sense_buf, sense_len);
}

/*
 * Format an INQUIRY CDB
 */
static int megasas_setup_inquiry(uint8_t *cdb, int pg, int len)
{
    memset(cdb, 0, 6);
    cdb[0] = INQUIRY;
    if (pg > 0) {
        cdb[1] = 0x1;
        cdb[2] = pg;
    }
    cdb[3] = (len >> 8) & 0xff;
    cdb[4] = (len & 0xff);
    return len;
}

/*
 * Encode lba and len into a READ_16/WRITE_16 CDB
 */
static void megasas_encode_lba(uint8_t *cdb, uint64_t lba,
                               uint32_t len, bool is_write)
{
    memset(cdb, 0x0, 16);
    if (is_write) {
        cdb[0] = WRITE_16;
    } else {
        cdb[0] = READ_16;
    }
    cdb[2] = (lba >> 56) & 0xff;
    cdb[3] = (lba >> 48) & 0xff;
    cdb[4] = (lba >> 40) & 0xff;
    cdb[5] = (lba >> 32) & 0xff;
    cdb[6] = (lba >> 24) & 0xff;
    cdb[7] = (lba >> 16) & 0xff;
    cdb[8] = (lba >> 8) & 0xff;
    cdb[9] = (lba) & 0xff;
    cdb[10] = (len >> 24) & 0xff;
    cdb[11] = (len >> 16) & 0xff;
    cdb[12] = (len >> 8) & 0xff;
    cdb[13] = (len) & 0xff;
}

/*
 * Utility functions
 */
static uint64_t megasas_fw_time(void)
{
    struct tm curtime;
    uint64_t bcd_time;

    vmx_get_timedate(&curtime, 0);
    bcd_time = ((uint64_t)curtime.tm_sec & 0xff) << 48 |
        ((uint64_t)curtime.tm_min & 0xff)  << 40 |
        ((uint64_t)curtime.tm_hour & 0xff) << 32 |
        ((uint64_t)curtime.tm_mday & 0xff) << 24 |
        ((uint64_t)curtime.tm_mon & 0xff)  << 16 |
        ((uint64_t)(curtime.tm_year + 1900) & 0xffff);

    return bcd_time;
}

/*
 * Default disk sata address
 * 0x1221 is the magic number as
 * present in real hardware,
 * so use it here, too.
 */
static uint64_t megasas_get_sata_addr(uint16_t id)
{
    uint64_t addr = (0x1221ULL << 48);
    return addr & (id << 24);
}

/*
 * Frame handling
 */
static int megasas_next_index(MegasasState *s, int index, int limit)
{
    index++;
    if (index == limit) {
        index = 0;
    }
    return index;
}

static MegasasCmd *megasas_lookup_frame(MegasasState *s,
    hwaddr frame)
{
    MegasasCmd *cmd = NULL;
    int num = 0, index;

    index = s->reply_queue_head;

    while (num < s->fw_cmds) {
        if (s->frames[index].pa && s->frames[index].pa == frame) {
            cmd = &s->frames[index];
            break;
        }
        index = megasas_next_index(s, index, s->fw_cmds);
        num++;
    }

    return cmd;
}

static void megasas_unmap_frame(MegasasState *s, MegasasCmd *cmd)
{
    PCIDevice *p = PCI_DEVICE(s);

    pci_dma_unmap(p, cmd->frame, cmd->pa_size, 0, 0);
    cmd->frame = NULL;
    cmd->pa = 0;
    clear_bit(cmd->index, s->frame_map);
}

/*
 * This absolutely needs to be locked if
 * qemu ever goes multithreaded.
 */
static MegasasCmd *megasas_enqueue_frame(MegasasState *s,
    hwaddr frame, uint64_t context, int count)
{
    PCIDevice *pcid = PCI_DEVICE(s);
    MegasasCmd *cmd = NULL;
    int frame_size = MFI_FRAME_SIZE * 16;
    hwaddr frame_size_p = frame_size;
    unsigned long index;

    index = 0;
    while (index < s->fw_cmds) {
        index = find_next_zero_bit(s->frame_map, s->fw_cmds, index);
        if (!s->frames[index].pa)
            break;
        /* Busy frame found */
    }
    if (index >= s->fw_cmds) {
        /* All frames busy */
        return NULL;
    }
    cmd = &s->frames[index];
    set_bit(index, s->frame_map);

    cmd->pa = frame;
    /* Map all possible frames */
    cmd->frame = pci_dma_map(pcid, frame, &frame_size_p, 0);
    if (frame_size_p != frame_size) {
        if (cmd->frame) {
            megasas_unmap_frame(s, cmd);
        }
        s->event_count++;
        return NULL;
    }
    cmd->pa_size = frame_size_p;
    cmd->context = context;
    if (!megasas_use_queue64(s)) {
        cmd->context &= (uint64_t)0xFFFFFFFF;
    }
    cmd->count = count;
    s->busy++;

    if (s->consumer_pa) {
        s->reply_queue_tail = ldl_le_phys(&address_space_memory,
                                          s->consumer_pa);
    }

    return cmd;
}

static void megasas_complete_frame(MegasasState *s, uint64_t context)
{
    PCIDevice *pci_dev = PCI_DEVICE(s);
    int tail, queue_offset;

    /* Decrement busy count */
    s->busy--;
    if (s->reply_queue_pa) {
        /*
         * Put command on the reply queue.
         * Context is opaque, but emulation is running in
         * little endian. So convert it.
         */
        if (megasas_use_queue64(s)) {
            queue_offset = s->reply_queue_head * sizeof(uint64_t);
            stq_le_phys(&address_space_memory,
                        s->reply_queue_pa + queue_offset, context);
        } else {
            queue_offset = s->reply_queue_head * sizeof(uint32_t);
            stl_le_phys(&address_space_memory,
                        s->reply_queue_pa + queue_offset, context);
        }
        s->reply_queue_tail = ldl_le_phys(&address_space_memory,
                                          s->consumer_pa);
    }

    if (megasas_intr_enabled(s)) {
        /* Update reply queue pointer */
        s->reply_queue_tail = ldl_le_phys(&address_space_memory,
                                          s->consumer_pa);
        tail = s->reply_queue_head;
        s->reply_queue_head = megasas_next_index(s, tail, s->fw_cmds);
        stl_le_phys(&address_space_memory,
                    s->producer_pa, s->reply_queue_head);
        /* Notify HBA */
        s->doorbell++;
        if (s->doorbell == 1) {
            pci_irq_assert(pci_dev);
        }
    }
}

static void megasas_reset_frames(MegasasState *s)
{
    int i;
    MegasasCmd *cmd;

    for (i = 0; i < s->fw_cmds; i++) {
        cmd = &s->frames[i];
        if (cmd->pa) {
            megasas_unmap_frame(s, cmd);
        }
    }
    bitmap_zero(s->frame_map, MEGASAS_MAX_FRAMES);
}

static void megasas_abort_command(MegasasCmd *cmd)
{
    if (cmd->req) {
        scsi_req_cancel(cmd->req);
        cmd->req = NULL;
    }
}

static int megasas_init_firmware(MegasasState *s, MegasasCmd *cmd)
{
    PCIDevice *pcid = PCI_DEVICE(s);
    uint32_t pa_hi, pa_lo;
    hwaddr iq_pa, initq_size = sizeof(struct mfi_init_qinfo);
    struct mfi_init_qinfo *initq = NULL;
    uint32_t flags;
    int ret = MFI_STAT_OK;

    if (s->reply_queue_pa) {
        goto out;
    }
    pa_lo = le32_to_cpu(cmd->frame->init.qinfo_new_addr_lo);
    pa_hi = le32_to_cpu(cmd->frame->init.qinfo_new_addr_hi);
    iq_pa = (((uint64_t) pa_hi << 32) | pa_lo);
    initq = pci_dma_map(pcid, iq_pa, &initq_size, 0);
    if (!initq || initq_size != sizeof(*initq)) {
        s->event_count++;
        ret = MFI_STAT_MEMORY_NOT_AVAILABLE;
        DPRINTF("megasas_init_firmware MFI_STAT_MEMORY_NOT_AVAILABLE\n");
        goto out;
    }
    s->reply_queue_len = le32_to_cpu(initq->rq_entries) & 0xFFFF;
    if (s->reply_queue_len > s->fw_cmds) {
        s->event_count++;
        ret = MFI_STAT_INVALID_PARAMETER;
        DPRINTF("megasas_init_firmware MFI_STAT_INVALID_PARAMETER\n");
        goto out;
    }
    pa_lo = le32_to_cpu(initq->rq_addr_lo);
    pa_hi = le32_to_cpu(initq->rq_addr_hi);
    s->reply_queue_pa = ((uint64_t) pa_hi << 32) | pa_lo;
    pa_lo = le32_to_cpu(initq->ci_addr_lo);
    pa_hi = le32_to_cpu(initq->ci_addr_hi);
    s->consumer_pa = ((uint64_t) pa_hi << 32) | pa_lo;
    pa_lo = le32_to_cpu(initq->pi_addr_lo);
    pa_hi = le32_to_cpu(initq->pi_addr_hi);
    s->producer_pa = ((uint64_t) pa_hi << 32) | pa_lo;
    s->reply_queue_head = ldl_le_phys(&address_space_memory, s->producer_pa);
    s->reply_queue_tail = ldl_le_phys(&address_space_memory, s->consumer_pa);
    flags = le32_to_cpu(initq->flags);
    if (flags & MFI_QUEUE_FLAG_CONTEXT64) {
        s->flags |= MEGASAS_MASK_USE_QUEUE64;
    }

    megasas_reset_frames(s);
    s->fw_state = MFI_FWSTATE_OPERATIONAL;
out:
    if (initq) {
        pci_dma_unmap(pcid, initq, initq_size, 0, 0);
    }
    return ret;
}

static int megasas_map_dcmd(MegasasState *s, MegasasCmd *cmd)
{
    uint64_t iov_pa, iov_size;

    cmd->flags = le16_to_cpu(cmd->frame->header.flags);
    if (!cmd->frame->header.sge_count) {
        cmd->iov_size = 0;
        return 0;
    } else if (cmd->frame->header.sge_count > 1) {
        cmd->iov_size = 0;
        return -1;
    }
    iov_pa = megasas_sgl_get_addr(cmd, &cmd->frame->dcmd.sgl);
    iov_size = megasas_sgl_get_len(cmd, &cmd->frame->dcmd.sgl);
    pci_dma_sglist_init(&cmd->qsg, PCI_DEVICE(s), 1);
    veertu_sglist_add(&cmd->qsg, iov_pa, iov_size);
    cmd->iov_size = iov_size;
    return cmd->iov_size;
}

static void megasas_finish_dcmd(MegasasCmd *cmd, uint32_t iov_size)
{
    if (cmd->frame->header.sge_count) {
        veertu_sglist_destroy(&cmd->qsg);
    }
    if (iov_size > cmd->iov_size) {
        if (megasas_frame_is_ieee_sgl(cmd)) {
            cmd->frame->dcmd.sgl.sg_skinny->len = cpu_to_le32(iov_size);
        } else if (megasas_frame_is_sgl64(cmd)) {
            cmd->frame->dcmd.sgl.sg64->len = cpu_to_le32(iov_size);
        } else {
            cmd->frame->dcmd.sgl.sg32->len = cpu_to_le32(iov_size);
        }
    }
    cmd->iov_size = 0;
}

static int megasas_ctrl_get_info(MegasasState *s, MegasasCmd *cmd)
{
    PCIDevice *pci_dev = PCI_DEVICE(s);
    PCIDeviceClass *pci_class = PCI_DEVICE_GET_CLASS(pci_dev);
    MegasasBaseClass *base_class = MEGASAS_DEVICE_GET_CLASS(s);
    struct mfi_ctrl_info info;
    size_t dcmd_size = sizeof(info);
    BusChild *kid;
    int num_pd_disks = 0;

    memset(&info, 0x0, cmd->iov_size);
    if (cmd->iov_size < dcmd_size) {
        DPRINTF("megasas_ctrl_get_info MFI_STAT_INVALID_PARAMETER\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    info.pci.vendor = cpu_to_le16(pci_class->vendor_id);
    info.pci.device = cpu_to_le16(pci_class->device_id);
    info.pci.subvendor = cpu_to_le16(pci_class->subsystem_vendor_id);
    info.pci.subdevice = cpu_to_le16(pci_class->subsystem_id);

    /*
     * For some reason the firmware supports
     * only up to 8 device ports.
     * Despite supporting a far larger number
     * of devices for the physical devices.
     * So just display the first 8 devices
     * in the device port list, independent
     * of how many logical devices are actually
     * present.
     */
    info.host.type = MFI_INFO_HOST_PCIE;
    info.device.type = MFI_INFO_DEV_SAS3G;
    info.device.port_count = 8;
    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);
        uint16_t pd_id;

        if (num_pd_disks < 8) {
            pd_id = ((sdev->id & 0xFF) << 8) | (sdev->lun & 0xFF);
            info.device.port_addr[num_pd_disks] =
                cpu_to_le64(megasas_get_sata_addr(pd_id));
        }
        num_pd_disks++;
    }

    memcpy(info.product_name, base_class->product_name, 24);
    snprintf(info.serial_number, 32, "%s", s->hba_serial);
    snprintf(info.package_version, 0x60, "%s-Veertu", QEMU_VERSION);
    memcpy(info.image_component[0].name, "APP", 3);
    snprintf(info.image_component[0].version, 10, "%s-Veertu",
             base_class->product_version);
    memcpy(info.image_component[0].build_date, "Aug  1 2015", 11);
    memcpy(info.image_component[0].build_time, "11:11:11", 8);
    info.image_component_count = 1;
    if (pci_dev->has_rom) {
        uint8_t biosver[32];
        uint8_t *ptr;

        ptr = memory_area_get_ram_ptr(&pci_dev->rom);
        memcpy(biosver, ptr + 0x41, 31);
        memcpy(info.image_component[1].name, "BIOS", 4);
        memcpy(info.image_component[1].version, biosver,
               strlen((const char *)biosver));
        info.image_component_count++;
    }
    info.current_fw_time = cpu_to_le32(megasas_fw_time());
    info.max_arms = 32;
    info.max_spans = 8;
    info.max_arrays = MEGASAS_MAX_ARRAYS;
    info.max_lds = MFI_MAX_LD;
    info.max_cmds = cpu_to_le16(s->fw_cmds);
    info.max_sg_elements = cpu_to_le16(s->fw_sge);
    info.max_request_size = cpu_to_le32(MEGASAS_MAX_SECTORS);
    if (!megasas_is_jbod(s))
        info.lds_present = cpu_to_le16(num_pd_disks);
    info.pd_present = cpu_to_le16(num_pd_disks);
    info.pd_disks_present = cpu_to_le16(num_pd_disks);
    info.hw_present = cpu_to_le32(MFI_INFO_HW_NVRAM |
                                   MFI_INFO_HW_MEM |
                                   MFI_INFO_HW_FLASH);
    info.memory_size = cpu_to_le16(512);
    info.nvram_size = cpu_to_le16(32);
    info.flash_size = cpu_to_le16(16);
    info.raid_levels = cpu_to_le32(MFI_INFO_RAID_0);
    info.adapter_ops = cpu_to_le32(MFI_INFO_AOPS_RBLD_RATE |
                                    MFI_INFO_AOPS_SELF_DIAGNOSTIC |
                                    MFI_INFO_AOPS_MIXED_ARRAY);
    info.ld_ops = cpu_to_le32(MFI_INFO_LDOPS_DISK_CACHE_POLICY |
                               MFI_INFO_LDOPS_ACCESS_POLICY |
                               MFI_INFO_LDOPS_IO_POLICY |
                               MFI_INFO_LDOPS_WRITE_POLICY |
                               MFI_INFO_LDOPS_READ_POLICY);
    info.max_strips_per_io = cpu_to_le16(s->fw_sge);
    info.stripe_sz_ops.min = 3;
    info.stripe_sz_ops.max = ffs(MEGASAS_MAX_SECTORS + 1) - 1;
    info.properties.pred_fail_poll_interval = cpu_to_le16(300);
    info.properties.intr_throttle_cnt = cpu_to_le16(16);
    info.properties.intr_throttle_timeout = cpu_to_le16(50);
    info.properties.rebuild_rate = 30;
    info.properties.patrol_read_rate = 30;
    info.properties.bgi_rate = 30;
    info.properties.cc_rate = 30;
    info.properties.recon_rate = 30;
    info.properties.cache_flush_interval = 4;
    info.properties.spinup_drv_cnt = 2;
    info.properties.spinup_delay = 6;
    info.properties.ecc_bucket_size = 15;
    info.properties.ecc_bucket_leak_rate = cpu_to_le16(1440);
    info.properties.expose_encl_devices = 1;
    info.properties.OnOffProperties = cpu_to_le32(MFI_CTRL_PROP_EnableJBOD);
    info.pd_ops = cpu_to_le32(MFI_INFO_PDOPS_FORCE_ONLINE |
                               MFI_INFO_PDOPS_FORCE_OFFLINE);
    info.pd_mix_support = cpu_to_le32(MFI_INFO_PDMIX_SAS |
                                       MFI_INFO_PDMIX_SATA |
                                       MFI_INFO_PDMIX_LD);

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_mfc_get_defaults(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_defaults info;
    size_t dcmd_size = sizeof(struct mfi_defaults);

    memset(&info, 0x0, dcmd_size);
    if (cmd->iov_size < dcmd_size) {
        DPRINTF("megasas_mfc_get_defaults MFI_STAT_INVALID_PARAMETER\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    info.sas_addr = cpu_to_le64(s->sas_addr);
    info.stripe_size = 3;
    info.flush_time = 4;
    info.background_rate = 30;
    info.allow_mix_in_enclosure = 1;
    info.allow_mix_in_ld = 1;
    info.direct_pd_mapping = 1;
    /* Enable for BIOS support */
    info.bios_enumerate_lds = 1;
    info.disable_ctrl_r = 1;
    info.expose_enclosure_devices = 1;
    info.disable_preboot_cli = 1;
    info.cluster_disable = 1;

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_get_bios_info(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_bios_data info;
    size_t dcmd_size = sizeof(info);

    memset(&info, 0x0, dcmd_size);
    if (cmd->iov_size < dcmd_size) {
        DPRINTF("megasas_dcmd_get_bios_info MFI_STAT_INVALID_PARAMETER\n");
        return MFI_STAT_INVALID_PARAMETER;
    }
    info.continue_on_error = 1;
    info.verbose = 1;
    if (megasas_is_jbod(s)) {
        info.expose_all_drives = 1;
    }

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_get_fw_time(MegasasState *s, MegasasCmd *cmd)
{
    uint64_t fw_time;
    size_t dcmd_size = sizeof(fw_time);

    fw_time = cpu_to_le64(megasas_fw_time());

    cmd->iov_size -= dma_buf_read((uint8_t *)&fw_time, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_set_fw_time(MegasasState *s, MegasasCmd *cmd)
{
    uint64_t fw_time;

    /* This is a dummy; setting of firmware time is not allowed */
    memcpy(&fw_time, cmd->frame->dcmd.mbox, sizeof(fw_time));

    fw_time = cpu_to_le64(megasas_fw_time());
    return MFI_STAT_OK;
}

static int megasas_event_info(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_evt_log_state info;
    size_t dcmd_size = sizeof(info);

    memset(&info, 0, dcmd_size);

    info.newest_seq_num = cpu_to_le32(s->event_count);
    info.shutdown_seq_num = cpu_to_le32(s->shutdown_event);
    info.boot_seq_num = cpu_to_le32(s->boot_event);

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_event_wait(MegasasState *s, MegasasCmd *cmd)
{
    union mfi_evt event;

    if (cmd->iov_size < sizeof(struct mfi_evt_detail)) {
        DPRINTF("megasas_event_wait MFI_STAT_INVALID_PARAMETER\n");
        return MFI_STAT_INVALID_PARAMETER;
    }
    s->event_count = cpu_to_le32(cmd->frame->dcmd.mbox[0]);
    event.word = cpu_to_le32(cmd->frame->dcmd.mbox[4]);
    s->event_locale = event.members.locale;
    s->event_class = event.members.class;
    s->event_cmd = cmd;
    /* Decrease busy count; event frame doesn't count here */
    s->busy--;
    cmd->iov_size = sizeof(struct mfi_evt_detail);
    return MFI_STAT_INVALID_STATUS;
}

static int megasas_dcmd_pd_get_list(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_pd_list info;
    size_t dcmd_size = sizeof(info);
    BusChild *kid;
    uint32_t offset, dcmd_limit, num_pd_disks = 0, max_pd_disks;

    memset(&info, 0, dcmd_size);
    offset = 8;
    dcmd_limit = offset + sizeof(struct mfi_pd_address);
    if (cmd->iov_size < dcmd_limit) {
        DPRINTF("megasas_dcmd_pd_get_list MFI_STAT_INVALID_PARAMETER\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    max_pd_disks = (cmd->iov_size - offset) / sizeof(struct mfi_pd_address);
    if (max_pd_disks > MFI_MAX_SYS_PDS) {
        max_pd_disks = MFI_MAX_SYS_PDS;
    }
    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);
        uint16_t pd_id;

        if (num_pd_disks >= max_pd_disks)
            break;

        pd_id = ((sdev->id & 0xFF) << 8) | (sdev->lun & 0xFF);
        info.addr[num_pd_disks].device_id = cpu_to_le16(pd_id);
        info.addr[num_pd_disks].encl_device_id = 0xFFFF;
        info.addr[num_pd_disks].encl_index = 0;
        info.addr[num_pd_disks].slot_number = sdev->id & 0xFF;
        info.addr[num_pd_disks].scsi_dev_type = sdev->type;
        info.addr[num_pd_disks].connect_port_bitmap = 0x1;
        info.addr[num_pd_disks].sas_addr[0] =
            cpu_to_le64(megasas_get_sata_addr(pd_id));
        num_pd_disks++;
        offset += sizeof(struct mfi_pd_address);
    }

    info.size = cpu_to_le32(offset);
    info.count = cpu_to_le32(num_pd_disks);

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, offset, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_pd_list_query(MegasasState *s, MegasasCmd *cmd)
{
    uint16_t flags;

    /* mbox0 contains flags */
    flags = le16_to_cpu(cmd->frame->dcmd.mbox[0]);
    if (flags == MR_PD_QUERY_TYPE_ALL ||
        megasas_is_jbod(s)) {
        return megasas_dcmd_pd_get_list(s, cmd);
    }

    return MFI_STAT_OK;
}

static int megasas_pd_get_info_submit(SCSIDevice *sdev, int lun,
                                      MegasasCmd *cmd)
{
    struct mfi_pd_info *info = cmd->iov_buf;
    size_t dcmd_size = sizeof(struct mfi_pd_info);
    uint64_t pd_size;
    uint16_t pd_id = ((sdev->id & 0xFF) << 8) | (lun & 0x7);
    uint8_t cmdbuf[6];
    SCSIRequest *req;
    size_t len, resid;

    if (!cmd->iov_buf) {
        cmd->iov_buf = g_malloc0(dcmd_size);
        info = cmd->iov_buf;
        info->inquiry_data[0] = 0x7f; /* Force PQual 0x3, PType 0x1f */
        info->vpd_page83[0] = 0x7f;
        megasas_setup_inquiry(cmdbuf, 0, sizeof(info->inquiry_data));
        req = scsi_req_new(sdev, cmd->index, lun, cmdbuf, cmd);
        if (!req) {
            g_free(cmd->iov_buf);
            cmd->iov_buf = NULL;
            DPRINTF("megasas_pd_get_info_submit scsi_req_new failed\n");
            return MFI_STAT_FLASH_ALLOC_FAIL;
        }

        len = scsi_req_enqueue(req);
        if (len > 0) {
            cmd->iov_size = len;
            scsi_req_continue(req);
        }
        DPRINTF("megasas_pd_get_info_submit len <=0\n");
        return MFI_STAT_INVALID_STATUS;
    } else if (info->inquiry_data[0] != 0x7f && info->vpd_page83[0] == 0x7f) {
        megasas_setup_inquiry(cmdbuf, 0x83, sizeof(info->vpd_page83));
        req = scsi_req_new(sdev, cmd->index, lun, cmdbuf, cmd);
        if (!req) {
            DPRINTF("megasas_pd_get_info_submit scsi_req_new failed\n");
            return MFI_STAT_FLASH_ALLOC_FAIL;
        }

        len = scsi_req_enqueue(req);
        if (len > 0) {
            cmd->iov_size = len;
            scsi_req_continue(req);
        }
        return MFI_STAT_INVALID_STATUS;
    }
    /* Finished, set FW state */
    if ((info->inquiry_data[0] >> 5) == 0) {
        if (megasas_is_jbod(cmd->state)) {
            info->fw_state = cpu_to_le16(MFI_PD_STATE_SYSTEM);
        } else {
            info->fw_state = cpu_to_le16(MFI_PD_STATE_ONLINE);
        }
    } else {
        info->fw_state = cpu_to_le16(MFI_PD_STATE_OFFLINE);
    }

    info->ref.v.device_id = cpu_to_le16(pd_id);
    info->state.ddf.pd_type = cpu_to_le16(MFI_PD_DDF_TYPE_IN_VD|
                                          MFI_PD_DDF_TYPE_INTF_SAS);
    blk_get_geometry(sdev->conf.blk, &pd_size);
    info->raw_size = cpu_to_le64(pd_size);
    info->non_coerced_size = cpu_to_le64(pd_size);
    info->coerced_size = cpu_to_le64(pd_size);
    info->encl_device_id = 0xFFFF;
    info->slot_number = (sdev->id & 0xFF);
    info->path_info.count = 1;
    info->path_info.sas_addr[0] =
        cpu_to_le64(megasas_get_sata_addr(pd_id));
    info->connected_port_bitmap = 0x1;
    info->device_speed = 1;
    info->link_speed = 1;
    resid = dma_buf_read(cmd->iov_buf, dcmd_size, &cmd->qsg);
    g_free(cmd->iov_buf);
    cmd->iov_size = dcmd_size - resid;
    cmd->iov_buf = NULL;
    return MFI_STAT_OK;
}

static int megasas_dcmd_pd_get_info(MegasasState *s, MegasasCmd *cmd)
{
    size_t dcmd_size = sizeof(struct mfi_pd_info);
    uint16_t pd_id;
    uint8_t target_id, lun_id;
    SCSIDevice *sdev = NULL;
    int retval = MFI_STAT_DEVICE_NOT_FOUND;

    if (cmd->iov_size < dcmd_size) {
        DPRINTF("megasas_dcmd_pd_get_info invalid iov_size\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    /* mbox0 has the ID */
    pd_id = le16_to_cpu(cmd->frame->dcmd.mbox[0]);
    target_id = (pd_id >> 8) & 0xFF;
    lun_id = pd_id & 7;
    sdev = scsi_device_find(&s->bus, 0, target_id, lun_id);

    if (sdev) {
        /* Submit inquiry */
        retval = megasas_pd_get_info_submit(sdev, pd_id, cmd);
    }

    return retval;
}

static int megasas_dcmd_ld_get_list(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_ld_list info;
    size_t dcmd_size = sizeof(info), resid;
    uint32_t num_ld_disks = 0, max_ld_disks;
    uint64_t ld_size = 0;
    BusChild *kid;

    memset(&info, 0, dcmd_size);
    if (cmd->iov_size > dcmd_size) {
        return MFI_STAT_INVALID_PARAMETER;
    }

    max_ld_disks = (cmd->iov_size - 8) / 16;
    if (megasas_is_jbod(s)) {
        max_ld_disks = 0;
    }
    if (max_ld_disks > MFI_MAX_LD) {
        max_ld_disks = MFI_MAX_LD;
    }
    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);

        if (num_ld_disks >= max_ld_disks) {
            break;
        }
        /* Logical device size is in blocks */
        if (sdev->conf.blk)
            blk_get_geometry(sdev->conf.blk, &ld_size);
        info.ld_list[num_ld_disks].ld.v.target_id = sdev->id;
        info.ld_list[num_ld_disks].state = MFI_LD_STATE_OPTIMAL;
        info.ld_list[num_ld_disks].size = cpu_to_le64(ld_size);
        num_ld_disks++;
    }
    info.ld_count = cpu_to_le32(num_ld_disks);

    resid = dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    cmd->iov_size = dcmd_size - resid;
    return MFI_STAT_OK;
}

static int megasas_dcmd_ld_list_query(MegasasState *s, MegasasCmd *cmd)
{
    uint16_t flags;
    struct mfi_ld_targetid_list info;
    size_t dcmd_size = sizeof(info), resid;
    uint32_t num_ld_disks = 0, max_ld_disks = s->fw_luns;
    BusChild *kid;

    /* mbox0 contains flags */
    flags = le16_to_cpu(cmd->frame->dcmd.mbox[0]);

    if (flags != MR_LD_QUERY_TYPE_ALL &&
        flags != MR_LD_QUERY_TYPE_EXPOSED_TO_HOST) {
        max_ld_disks = 0;
    }

    memset(&info, 0, dcmd_size);
    if (cmd->iov_size < 12) {
        DPRINTF("megasas_dcmd_ld_list_query cmd->iov_size < 12\n");
        return MFI_STAT_INVALID_PARAMETER;
    }
    dcmd_size = sizeof(uint32_t) * 2 + 3;
    max_ld_disks = cmd->iov_size - dcmd_size;
    if (megasas_is_jbod(s)) {
        max_ld_disks = 0;
    }
    if (max_ld_disks > MFI_MAX_LD) {
        max_ld_disks = MFI_MAX_LD;
    }
    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);

        if (num_ld_disks >= max_ld_disks) {
            break;
        }
        info.targetid[num_ld_disks] = sdev->lun;
        num_ld_disks++;
        dcmd_size++;
    }
    info.ld_count = cpu_to_le32(num_ld_disks);
    info.size = dcmd_size;

    resid = dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    cmd->iov_size = dcmd_size - resid;
    return MFI_STAT_OK;
}

static int megasas_ld_get_info_submit(SCSIDevice *sdev, int lun,
                                      MegasasCmd *cmd)
{
    struct mfi_ld_info *info = cmd->iov_buf;
    size_t dcmd_size = sizeof(struct mfi_ld_info);
    uint8_t cdb[6];
    SCSIRequest *req;
    ssize_t len, resid;
    uint16_t sdev_id = ((sdev->id & 0xFF) << 8) | (lun & 0xFF);
    uint64_t ld_size;

    if (!cmd->iov_buf) {
        cmd->iov_buf = g_malloc0(dcmd_size);
        info = cmd->iov_buf;
        megasas_setup_inquiry(cdb, 0x83, sizeof(info->vpd_page83));
        req = scsi_req_new(sdev, cmd->index, lun, cdb, cmd);
        if (!req) {
            g_free(cmd->iov_buf);
            cmd->iov_buf = NULL;
            DPRINTF("megasas_ld_get_info_submit alloc failed\n");
            return MFI_STAT_FLASH_ALLOC_FAIL;
        }

        len = scsi_req_enqueue(req);
        if (len > 0) {
            cmd->iov_size = len;
            scsi_req_continue(req);
        }
        DPRINTF("megasas_ld_get_info_submit len < 0\n");
        return MFI_STAT_INVALID_STATUS;
    }

    info->ld_config.params.state = MFI_LD_STATE_OPTIMAL;
    info->ld_config.properties.ld.v.target_id = lun;
    info->ld_config.params.stripe_size = 3;
    info->ld_config.params.num_drives = 1;
    info->ld_config.params.is_consistent = 1;
    /* Logical device size is in blocks */
    blk_get_geometry(sdev->conf.blk, &ld_size);
    info->size = cpu_to_le64(ld_size);
    memset(info->ld_config.span, 0, sizeof(info->ld_config.span));
    info->ld_config.span[0].start_block = 0;
    info->ld_config.span[0].num_blocks = info->size;
    info->ld_config.span[0].array_ref = cpu_to_le16(sdev_id);

    resid = dma_buf_read(cmd->iov_buf, dcmd_size, &cmd->qsg);
    g_free(cmd->iov_buf);
    cmd->iov_size = dcmd_size - resid;
    cmd->iov_buf = NULL;
    return MFI_STAT_OK;
}

static int megasas_dcmd_ld_get_info(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_ld_info info;
    size_t dcmd_size = sizeof(info);
    uint16_t ld_id;
    uint32_t max_ld_disks = s->fw_luns;
    SCSIDevice *sdev = NULL;
    int retval = MFI_STAT_DEVICE_NOT_FOUND;

    if (cmd->iov_size < dcmd_size) {
        DPRINTF("megasas_dcmd_ld_get_info invalid iov_size\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    /* mbox0 has the ID */
    ld_id = le16_to_cpu(cmd->frame->dcmd.mbox[0]);

    if (megasas_is_jbod(s)) {
        return MFI_STAT_DEVICE_NOT_FOUND;
    }

    if (ld_id < max_ld_disks) {
        sdev = scsi_device_find(&s->bus, 0, ld_id, 0);
    }

    if (sdev) {
        retval = megasas_ld_get_info_submit(sdev, ld_id, cmd);
    }

    return retval;
}

static int megasas_dcmd_cfg_read(MegasasState *s, MegasasCmd *cmd)
{
    uint8_t data[4096];
    struct mfi_config_data *info;
    int num_pd_disks = 0, array_offset, ld_offset;
    BusChild *kid;

    if (cmd->iov_size > 4096) {
        DPRINTF("megasas_dcmd_cfg_read invalid iov_size\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        num_pd_disks++;
    }
    info = (struct mfi_config_data *)&data;
    /*
     * Array mapping:
     * - One array per SCSI device
     * - One logical drive per SCSI device
     *   spanning the entire device
     */
    info->array_count = num_pd_disks;
    info->array_size = sizeof(struct mfi_array) * num_pd_disks;
    info->log_drv_count = num_pd_disks;
    info->log_drv_size = sizeof(struct mfi_ld_config) * num_pd_disks;
    info->spares_count = 0;
    info->spares_size = sizeof(struct mfi_spare);
    info->size = sizeof(struct mfi_config_data) + info->array_size +
        info->log_drv_size;
    if (info->size > 4096) {
        DPRINTF("megasas_dcmd_cfg_read invalid info->size\n");
        return MFI_STAT_INVALID_PARAMETER;
    }

    array_offset = sizeof(struct mfi_config_data);
    ld_offset = array_offset + sizeof(struct mfi_array) * num_pd_disks;

    QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
        SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);
        uint16_t sdev_id = ((sdev->id & 0xFF) << 8) | (sdev->lun & 0xFF);
        struct mfi_array *array;
        struct mfi_ld_config *ld;
        uint64_t pd_size;
        int i;

        array = (struct mfi_array *)(data + array_offset);
        blk_get_geometry(sdev->conf.blk, &pd_size);
        array->size = cpu_to_le64(pd_size);
        array->num_drives = 1;
        array->array_ref = cpu_to_le16(sdev_id);
        array->pd[0].ref.v.device_id = cpu_to_le16(sdev_id);
        array->pd[0].ref.v.seq_num = 0;
        array->pd[0].fw_state = MFI_PD_STATE_ONLINE;
        array->pd[0].encl.pd = 0xFF;
        array->pd[0].encl.slot = (sdev->id & 0xFF);
        for (i = 1; i < MFI_MAX_ROW_SIZE; i++) {
            array->pd[i].ref.v.device_id = 0xFFFF;
            array->pd[i].ref.v.seq_num = 0;
            array->pd[i].fw_state = MFI_PD_STATE_UNCONFIGURED_GOOD;
            array->pd[i].encl.pd = 0xFF;
            array->pd[i].encl.slot = 0xFF;
        }
        array_offset += sizeof(struct mfi_array);
        ld = (struct mfi_ld_config *)(data + ld_offset);
        memset(ld, 0, sizeof(struct mfi_ld_config));
        ld->properties.ld.v.target_id = sdev->id;
        ld->properties.default_cache_policy = MR_LD_CACHE_READ_AHEAD |
            MR_LD_CACHE_READ_ADAPTIVE;
        ld->properties.current_cache_policy = MR_LD_CACHE_READ_AHEAD |
            MR_LD_CACHE_READ_ADAPTIVE;
        ld->params.state = MFI_LD_STATE_OPTIMAL;
        ld->params.stripe_size = 3;
        ld->params.num_drives = 1;
        ld->params.span_depth = 1;
        ld->params.is_consistent = 1;
        ld->span[0].start_block = 0;
        ld->span[0].num_blocks = cpu_to_le64(pd_size);
        ld->span[0].array_ref = cpu_to_le16(sdev_id);
        ld_offset += sizeof(struct mfi_ld_config);
    }

    cmd->iov_size -= dma_buf_read((uint8_t *)data, info->size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_get_properties(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_ctrl_props info;
    size_t dcmd_size = sizeof(info);

    memset(&info, 0x0, dcmd_size);
    if (cmd->iov_size < dcmd_size) {
        return MFI_STAT_INVALID_PARAMETER;
    }
    info.pred_fail_poll_interval = cpu_to_le16(300);
    info.intr_throttle_cnt = cpu_to_le16(16);
    info.intr_throttle_timeout = cpu_to_le16(50);
    info.rebuild_rate = 30;
    info.patrol_read_rate = 30;
    info.bgi_rate = 30;
    info.cc_rate = 30;
    info.recon_rate = 30;
    info.cache_flush_interval = 4;
    info.spinup_drv_cnt = 2;
    info.spinup_delay = 6;
    info.ecc_bucket_size = 15;
    info.ecc_bucket_leak_rate = cpu_to_le16(1440);
    info.expose_encl_devices = 1;

    cmd->iov_size -= dma_buf_read((uint8_t *)&info, dcmd_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_cache_flush(MegasasState *s, MegasasCmd *cmd)
{
    blk_drain_all();
    return MFI_STAT_OK;
}

static int megasas_ctrl_shutdown(MegasasState *s, MegasasCmd *cmd)
{
    s->fw_state = MFI_FWSTATE_READY;
    return MFI_STAT_OK;
}

/* Some implementations use CLUSTER RESET LD to simulate a device reset */
static int megasas_cluster_reset_ld(MegasasState *s, MegasasCmd *cmd)
{
    uint16_t target_id;
    int i;

    /* mbox0 contains the device index */
    target_id = le16_to_cpu(cmd->frame->dcmd.mbox[0]);
    for (i = 0; i < s->fw_cmds; i++) {
        MegasasCmd *tmp_cmd = &s->frames[i];
        if (tmp_cmd->req && tmp_cmd->req->dev->id == target_id) {
            SCSIDevice *d = tmp_cmd->req->dev;
            qdev_reset_all(&d->qdev);
        }
    }
    return MFI_STAT_OK;
}

static int megasas_dcmd_set_properties(MegasasState *s, MegasasCmd *cmd)
{
    struct mfi_ctrl_props info;
    size_t dcmd_size = sizeof(info);

    if (cmd->iov_size < dcmd_size) {
        return MFI_STAT_INVALID_PARAMETER;
    }
    dma_buf_write((uint8_t *)&info, cmd->iov_size, &cmd->qsg);
    return MFI_STAT_OK;
}

static int megasas_dcmd_dummy(MegasasState *s, MegasasCmd *cmd)
{
    return MFI_STAT_OK;
}

static const struct dcmd_cmd_tbl_t {
    int opcode;
    const char *desc;
    int (*func)(MegasasState *s, MegasasCmd *cmd);
} dcmd_cmd_tbl[] = {
    { MFI_DCMD_CTRL_MFI_HOST_MEM_ALLOC, "CTRL_HOST_MEM_ALLOC",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_GET_INFO, "CTRL_GET_INFO",
      megasas_ctrl_get_info },
    { MFI_DCMD_CTRL_GET_PROPERTIES, "CTRL_GET_PROPERTIES",
      megasas_dcmd_get_properties },
    { MFI_DCMD_CTRL_SET_PROPERTIES, "CTRL_SET_PROPERTIES",
      megasas_dcmd_set_properties },
    { MFI_DCMD_CTRL_ALARM_GET, "CTRL_ALARM_GET",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_ALARM_ENABLE, "CTRL_ALARM_ENABLE",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_ALARM_DISABLE, "CTRL_ALARM_DISABLE",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_ALARM_SILENCE, "CTRL_ALARM_SILENCE",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_ALARM_TEST, "CTRL_ALARM_TEST",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_EVENT_GETINFO, "CTRL_EVENT_GETINFO",
      megasas_event_info },
    { MFI_DCMD_CTRL_EVENT_GET, "CTRL_EVENT_GET",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_EVENT_WAIT, "CTRL_EVENT_WAIT",
      megasas_event_wait },
    { MFI_DCMD_CTRL_SHUTDOWN, "CTRL_SHUTDOWN",
      megasas_ctrl_shutdown },
    { MFI_DCMD_HIBERNATE_STANDBY, "CTRL_STANDBY",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_GET_TIME, "CTRL_GET_TIME",
      megasas_dcmd_get_fw_time },
    { MFI_DCMD_CTRL_SET_TIME, "CTRL_SET_TIME",
      megasas_dcmd_set_fw_time },
    { MFI_DCMD_CTRL_BIOS_DATA_GET, "CTRL_BIOS_DATA_GET",
      megasas_dcmd_get_bios_info },
    { MFI_DCMD_CTRL_FACTORY_DEFAULTS, "CTRL_FACTORY_DEFAULTS",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_MFC_DEFAULTS_GET, "CTRL_MFC_DEFAULTS_GET",
      megasas_mfc_get_defaults },
    { MFI_DCMD_CTRL_MFC_DEFAULTS_SET, "CTRL_MFC_DEFAULTS_SET",
      megasas_dcmd_dummy },
    { MFI_DCMD_CTRL_CACHE_FLUSH, "CTRL_CACHE_FLUSH",
      megasas_cache_flush },
    { MFI_DCMD_PD_GET_LIST, "PD_GET_LIST",
      megasas_dcmd_pd_get_list },
    { MFI_DCMD_PD_LIST_QUERY, "PD_LIST_QUERY",
      megasas_dcmd_pd_list_query },
    { MFI_DCMD_PD_GET_INFO, "PD_GET_INFO",
      megasas_dcmd_pd_get_info },
    { MFI_DCMD_PD_STATE_SET, "PD_STATE_SET",
      megasas_dcmd_dummy },
    { MFI_DCMD_PD_REBUILD, "PD_REBUILD",
      megasas_dcmd_dummy },
    { MFI_DCMD_PD_BLINK, "PD_BLINK",
      megasas_dcmd_dummy },
    { MFI_DCMD_PD_UNBLINK, "PD_UNBLINK",
      megasas_dcmd_dummy },
    { MFI_DCMD_LD_GET_LIST, "LD_GET_LIST",
      megasas_dcmd_ld_get_list},
    { MFI_DCMD_LD_LIST_QUERY, "LD_LIST_QUERY",
      megasas_dcmd_ld_list_query },
    { MFI_DCMD_LD_GET_INFO, "LD_GET_INFO",
      megasas_dcmd_ld_get_info },
    { MFI_DCMD_LD_GET_PROP, "LD_GET_PROP",
      megasas_dcmd_dummy },
    { MFI_DCMD_LD_SET_PROP, "LD_SET_PROP",
      megasas_dcmd_dummy },
    { MFI_DCMD_LD_DELETE, "LD_DELETE",
      megasas_dcmd_dummy },
    { MFI_DCMD_CFG_READ, "CFG_READ",
      megasas_dcmd_cfg_read },
    { MFI_DCMD_CFG_ADD, "CFG_ADD",
      megasas_dcmd_dummy },
    { MFI_DCMD_CFG_CLEAR, "CFG_CLEAR",
      megasas_dcmd_dummy },
    { MFI_DCMD_CFG_FOREIGN_READ, "CFG_FOREIGN_READ",
      megasas_dcmd_dummy },
    { MFI_DCMD_CFG_FOREIGN_IMPORT, "CFG_FOREIGN_IMPORT",
      megasas_dcmd_dummy },
    { MFI_DCMD_BBU_STATUS, "BBU_STATUS",
      megasas_dcmd_dummy },
    { MFI_DCMD_BBU_CAPACITY_INFO, "BBU_CAPACITY_INFO",
      megasas_dcmd_dummy },
    { MFI_DCMD_BBU_DESIGN_INFO, "BBU_DESIGN_INFO",
      megasas_dcmd_dummy },
    { MFI_DCMD_BBU_PROP_GET, "BBU_PROP_GET",
      megasas_dcmd_dummy },
    { MFI_DCMD_CLUSTER, "CLUSTER",
      megasas_dcmd_dummy },
    { MFI_DCMD_CLUSTER_RESET_ALL, "CLUSTER_RESET_ALL",
      megasas_dcmd_dummy },
    { MFI_DCMD_CLUSTER_RESET_LD, "CLUSTER_RESET_LD",
      megasas_cluster_reset_ld },
    { -1, NULL, NULL }
};

static int megasas_handle_dcmd(MegasasState *s, MegasasCmd *cmd)
{
    int opcode, len;
    int retval = 0;
    const struct dcmd_cmd_tbl_t *cmdptr = dcmd_cmd_tbl;

    opcode = le32_to_cpu(cmd->frame->dcmd.opcode);
    len = megasas_map_dcmd(s, cmd);
    if (len < 0) {
        return MFI_STAT_MEMORY_NOT_AVAILABLE;
    }
    while (cmdptr->opcode != -1 && cmdptr->opcode != opcode) {
        cmdptr++;
    }
    if (cmdptr->opcode == -1) {
        retval = megasas_dcmd_dummy(s, cmd);
    } else {
        retval = cmdptr->func(s, cmd);
    }
    if (retval != MFI_STAT_INVALID_STATUS) {
        megasas_finish_dcmd(cmd, len);
    }
    return retval;
}

static int megasas_finish_internal_dcmd(MegasasCmd *cmd,
                                        SCSIRequest *req)
{
    int opcode;
    int retval = MFI_STAT_OK;
    int lun = req->lun;

    opcode = le32_to_cpu(cmd->frame->dcmd.opcode);
    scsi_req_unref(req);
    switch (opcode) {
    case MFI_DCMD_PD_GET_INFO:
        retval = megasas_pd_get_info_submit(req->dev, lun, cmd);
        break;
    case MFI_DCMD_LD_GET_INFO:
        retval = megasas_ld_get_info_submit(req->dev, lun, cmd);
        break;
    default:
        retval = MFI_STAT_INVALID_DCMD;
        break;
    }
    if (retval != MFI_STAT_INVALID_STATUS) {
        megasas_finish_dcmd(cmd, cmd->iov_size);
    }
    return retval;
}

static int megasas_enqueue_req(MegasasCmd *cmd, bool is_write)
{
    int len;

    len = scsi_req_enqueue(cmd->req);
    if (len < 0) {
        len = -len;
    }
    if (len > 0) {
        if (len > cmd->iov_size) {
        }
        if (len < cmd->iov_size) {
            cmd->iov_size = len;
        }
        scsi_req_continue(cmd->req);
    }
    return len;
}

static int megasas_handle_scsi(MegasasState *s, MegasasCmd *cmd,
                               bool is_logical)
{
    uint8_t *cdb;
    bool is_write;
    struct SCSIDevice *sdev = NULL;
    int lun_id = cmd->frame->header.lun_id & 7;

    cdb = cmd->frame->pass.cdb;
    int cdb_len = cmd->frame->header.cdb_len;

    if (is_logical) {
        if (cmd->frame->header.target_id >= MFI_MAX_LD ||
            lun_id != 0) {
            return MFI_STAT_DEVICE_NOT_FOUND;
        }
    }
    sdev = scsi_device_find(&s->bus, 0, cmd->frame->header.target_id,
                            cmd->frame->header.lun_id);

    cmd->iov_size = le32_to_cpu(cmd->frame->header.data_len);

    if (!sdev || (megasas_is_jbod(s) && is_logical)) {
        return MFI_STAT_DEVICE_NOT_FOUND;
    }

    if (megasas_map_sgl(s, cmd, &cmd->frame->pass.sgl)) {
        megasas_write_sense(cmd, SENSE_CODE(TARGET_FAILURE));
        cmd->frame->header.scsi_status = CHECK_CONDITION;
        s->event_count++;
        DPRINTF("megasas_handle_scsi error\n");
        return MFI_STAT_SCSI_DONE_WITH_ERROR;
    }

    cmd->req = scsi_req_new(sdev, cmd->index, lun_id, cdb, cmd);
    if (!cmd->req) {
        megasas_write_sense(cmd, SENSE_CODE(NO_SENSE));
        cmd->frame->header.scsi_status = BUSY;
        s->event_count++;
        return MFI_STAT_SCSI_DONE_WITH_ERROR;
    }

    is_write = (cmd->req->cmd.mode == SCSI_XFER_TO_DEV);
    megasas_enqueue_req(cmd, is_write);
    return MFI_STAT_INVALID_STATUS;
}

static int megasas_handle_io(MegasasState *s, MegasasCmd *cmd)
{
    uint32_t lba_count, lba_start_hi, lba_start_lo;
    uint64_t lba_start;
    bool is_write = (cmd->frame->header.frame_cmd == MFI_CMD_LD_WRITE);
    uint8_t cdb[16];
    int len;
    struct SCSIDevice *sdev = NULL;
    int lun_id = cmd->frame->header.lun_id & 7;

    lba_count = le32_to_cpu(cmd->frame->io.header.data_len);
    lba_start_lo = le32_to_cpu(cmd->frame->io.lba_lo);
    lba_start_hi = le32_to_cpu(cmd->frame->io.lba_hi);
    lba_start = ((uint64_t)lba_start_hi << 32) | lba_start_lo;

    if (cmd->frame->header.target_id < MFI_MAX_LD && lun_id == 0) {
        sdev = scsi_device_find(&s->bus, 0, cmd->frame->header.target_id,
                                cmd->frame->header.lun_id);
    }
    if (!sdev) {
        DPRINTF("megasas_handle_io !sdev, target %d, lun %d\n", cmd->frame->header.target_id, lun_id);
        return MFI_STAT_DEVICE_NOT_FOUND;
    }

    cmd->iov_size = lba_count * sdev->blocksize;
    if (megasas_map_sgl(s, cmd, &cmd->frame->io.sgl)) {
        megasas_write_sense(cmd, SENSE_CODE(TARGET_FAILURE));
        cmd->frame->header.scsi_status = CHECK_CONDITION;
        s->event_count++;
        DPRINTF("megasas_handle_io error\n");
        return MFI_STAT_SCSI_DONE_WITH_ERROR;
    }

    megasas_encode_lba(cdb, lba_start, lba_count, is_write);
    cmd->req = scsi_req_new(sdev, cmd->index, lun_id, cdb, cmd);
    if (!cmd->req) {
        megasas_write_sense(cmd, SENSE_CODE(NO_SENSE));
        cmd->frame->header.scsi_status = BUSY;
        s->event_count++;
        DPRINTF("megasas_handle_io !req\n");
        return MFI_STAT_SCSI_DONE_WITH_ERROR;
    }
    len = megasas_enqueue_req(cmd, is_write);
    return MFI_STAT_INVALID_STATUS;
}

static int megasas_finish_internal_command(MegasasCmd *cmd,
                                           SCSIRequest *req, size_t resid)
{
    int retval = MFI_STAT_INVALID_CMD;

    if (cmd->frame->header.frame_cmd == MFI_CMD_DCMD) {
        cmd->iov_size -= resid;
        retval = megasas_finish_internal_dcmd(cmd, req);
    }
    return retval;
}

static VeertuSGList *megasas_get_sg_list(SCSIRequest *req)
{
    MegasasCmd *cmd = req->hba_private;

    if (cmd->frame->header.frame_cmd == MFI_CMD_DCMD) {
        return NULL;
    } else {
        return &cmd->qsg;
    }
}

static void megasas_xfer_complete(SCSIRequest *req, uint32_t len)
{
    MegasasCmd *cmd = req->hba_private;
    uint8_t *buf;
    uint32_t opcode;

    if (cmd->frame->header.frame_cmd != MFI_CMD_DCMD) {
        scsi_req_continue(req);
        return;
    }

    buf = scsi_req_get_buf(req);
    opcode = le32_to_cpu(cmd->frame->dcmd.opcode);
    if (opcode == MFI_DCMD_PD_GET_INFO && cmd->iov_buf) {
        struct mfi_pd_info *info = cmd->iov_buf;

        if (info->inquiry_data[0] == 0x7f) {
            memset(info->inquiry_data, 0, sizeof(info->inquiry_data));
            memcpy(info->inquiry_data, buf, len);
        } else if (info->vpd_page83[0] == 0x7f) {
            memset(info->vpd_page83, 0, sizeof(info->vpd_page83));
            memcpy(info->vpd_page83, buf, len);
        }
        scsi_req_continue(req);
    } else if (opcode == MFI_DCMD_LD_GET_INFO) {
        struct mfi_ld_info *info = cmd->iov_buf;

        if (cmd->iov_buf) {
            memcpy(info->vpd_page83, buf, sizeof(info->vpd_page83));
            scsi_req_continue(req);
        }
    }
}

static void megasas_command_complete(SCSIRequest *req, uint32_t status,
                                     size_t resid)
{
    MegasasCmd *cmd = req->hba_private;
    uint8_t cmd_status = MFI_STAT_OK;

    if (cmd->req != req) {
        /*
         * Internal command complete
         */
        cmd_status = megasas_finish_internal_command(cmd, req, resid);
        DPRINTF("Internal command complete %d\n", cmd_status);
        if (cmd_status == MFI_STAT_INVALID_STATUS) {
            return;
        }
    } else {
        req->status = status;
        if (req->status != GOOD) {
            DPRINTF("megasas_command_complete status %d\n", req->status);
            cmd_status = MFI_STAT_SCSI_DONE_WITH_ERROR;
        }
        if (req->status == CHECK_CONDITION) {
            megasas_copy_sense(cmd);
        }

        megasas_unmap_sgl(cmd);
        cmd->frame->header.scsi_status = req->status;
        scsi_req_unref(cmd->req);
        cmd->req = NULL;
    }
    cmd->frame->header.cmd_status = cmd_status;
    megasas_unmap_frame(cmd->state, cmd);
    megasas_complete_frame(cmd->state, cmd->context);
}

static void megasas_command_cancel(SCSIRequest *req)
{
    DPRINTF("megasas_command_cancel\n");
    MegasasCmd *cmd = req->hba_private;

    if (cmd) {
        megasas_abort_command(cmd);
    } else {
        scsi_req_unref(req);
    }
}

static int megasas_handle_abort(MegasasState *s, MegasasCmd *cmd)
{
    uint64_t abort_ctx = le64_to_cpu(cmd->frame->abort.abort_context);
    hwaddr abort_addr, addr_hi, addr_lo;
    MegasasCmd *abort_cmd;

    DPRINTF("megasas_handle_abort\n");

    addr_hi = le32_to_cpu(cmd->frame->abort.abort_mfi_addr_hi);
    addr_lo = le32_to_cpu(cmd->frame->abort.abort_mfi_addr_lo);
    abort_addr = ((uint64_t)addr_hi << 32) | addr_lo;

    abort_cmd = megasas_lookup_frame(s, abort_addr);
    if (!abort_cmd) {
        s->event_count++;
        return MFI_STAT_OK;
    }
    if (!megasas_use_queue64(s)) {
        abort_ctx &= (uint64_t)0xFFFFFFFF;
    }
    if (abort_cmd->context != abort_ctx) {
        s->event_count++;
        return MFI_STAT_ABORT_NOT_POSSIBLE;
    }
    megasas_abort_command(abort_cmd);
    if (!s->event_cmd || abort_cmd != s->event_cmd) {
        s->event_cmd = NULL;
    }
    s->event_count++;
    return MFI_STAT_OK;
}

static void megasas_handle_frame(MegasasState *s, uint64_t frame_addr,
                                 uint32_t frame_count)
{
    uint8_t frame_status = MFI_STAT_INVALID_CMD;
    uint64_t frame_context;
    MegasasCmd *cmd;

    /*
     * Always read 64bit context, top bits will be
     * masked out if required in megasas_enqueue_frame()
     */
    frame_context = megasas_frame_get_context(frame_addr);

    cmd = megasas_enqueue_frame(s, frame_addr, frame_context, frame_count);
    if (!cmd) {
        /* reply queue full */
        megasas_frame_set_scsi_status(frame_addr, BUSY);
        megasas_frame_set_cmd_status(frame_addr, MFI_STAT_SCSI_DONE_WITH_ERROR);
        megasas_complete_frame(s, frame_context);
        s->event_count++;
        return;
    }
    switch (cmd->frame->header.frame_cmd) {
    case MFI_CMD_INIT:
        DPRINTF("handle_frame MFI_CMD_INIT\n");
        frame_status = megasas_init_firmware(s, cmd);
        break;
    case MFI_CMD_DCMD:
        DPRINTF("handle_frame MFI_CMD_DCMD\n");
        frame_status = megasas_handle_dcmd(s, cmd);
        break;
    case MFI_CMD_ABORT:
        DPRINTF("handle_frame MFI_CMD_ABORT\n");
        frame_status = megasas_handle_abort(s, cmd);
        break;
    case MFI_CMD_PD_SCSI_IO:
        DPRINTF("handle_frame MFI_CMD_PD_SCSI_IO\n");
        frame_status = megasas_handle_scsi(s, cmd, 0);
        break;
    case MFI_CMD_LD_SCSI_IO:
        DPRINTF("handle_frame MFI_CMD_LD_SCSI_IO\n");
        frame_status = megasas_handle_scsi(s, cmd, 1);
        break;
    case MFI_CMD_LD_READ:
    case MFI_CMD_LD_WRITE:
        //DPRINTF("handle_frame MFI_CMD_LD_READ/WRITE\n");
        frame_status = megasas_handle_io(s, cmd);
        break;
    default:
        DPRINTF("handle_frame cmd %d!!!\n", cmd->frame->header.frame_cmd);
        s->event_count++;
        break;
    }
    if (frame_status != MFI_STAT_INVALID_STATUS) {
        if (cmd->frame) {
            cmd->frame->header.cmd_status = frame_status;
        } else {
            megasas_frame_set_cmd_status(frame_addr, frame_status);
        }
        megasas_unmap_frame(s, cmd);
        megasas_complete_frame(s, cmd->context);
    }
}

static uint64_t megasas_mmio_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    MegasasState *s = opaque;
    PCIDevice *pci_dev = PCI_DEVICE(s);
    MegasasBaseClass *base_class = MEGASAS_DEVICE_GET_CLASS(s);
    uint32_t retval = 0;

    switch (addr) {
    case MFI_IDB:
        DPRINTF("mmio_read MFI_IDB\n");
        retval = 0;
        break;
    case MFI_OMSG0:
        DPRINTF("mmio_read MFI_OMSG0\n");
    case MFI_OSP0:
        //DPRINTF("mmio_read MFI_OSP0\n");
        retval = (0) |
            (s->fw_state & MFI_FWSTATE_MASK) |
            ((s->fw_sge & 0xff) << 16) |
            (s->fw_cmds & 0xFFFF);
        break;
    case MFI_OSTS:
        //DPRINTF("mmio_read MFI_OSTS\n");
        if (megasas_intr_enabled(s) && s->doorbell) {
            retval = base_class->osts;
        }
        break;
    case MFI_OMSK:
        //DPRINTF("mmio_read MFI_OMSK\n");
        retval = s->intr_mask;
        break;
    case MFI_ODCR0:
        //DPRINTF("mmio_read MFI_ODCR0\n");
        retval = s->doorbell ? 1 : 0;
        break;
    case MFI_DIAG:
        DPRINTF("mmio_read MFI_DIAG\n");
        retval = s->diag;
        break;
    case MFI_OSP1:
        //DPRINTF("mmio_read MFI_OSP1\n");
        retval = 15;
        break;
    default:
        DPRINTF("mmio_read addr %d!!!\n", addr);
        break;
    }
    return retval;
}

static int adp_reset_seq[] = {0x00, 0x04, 0x0b, 0x02, 0x07, 0x0d};

static void megasas_mmio_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned size)
{
    MegasasState *s = opaque;
    PCIDevice *pci_dev = PCI_DEVICE(s);
    uint64_t frame_addr;
    uint32_t frame_count;
    int i;

    switch (addr) {
    case MFI_IDB:
        DPRINTF("mmio_write MFI_IDB\n");
        if (val & MFI_FWINIT_ABORT) {
            /* Abort all pending cmds */
            for (i = 0; i < s->fw_cmds; i++) {
                megasas_abort_command(&s->frames[i]);
            }
        }
        if (val & MFI_FWINIT_READY) {
            /* move to FW READY */
            megasas_soft_reset(s);
        }
        if (val & MFI_FWINIT_MFIMODE) {
            /* discard MFIs */
        }
        if (val & MFI_FWINIT_STOP_ADP) {
            /* Terminal error, stop processing */
            s->fw_state = MFI_FWSTATE_FAULT;
        }
        break;
    case MFI_OMSK:
        DPRINTF("mmio_write MFI_OMSK\n");
        s->intr_mask = val;
        if (!megasas_intr_enabled(s)) {
            pci_irq_deassert(pci_dev);
        }
        if (megasas_intr_enabled(s)) {
        } else {
            megasas_soft_reset(s);
        }
        break;
    case MFI_ODCR0:
        //DPRINTF("mmio_write MFI_ODCR0\n");
        s->doorbell = 0;
        if (megasas_intr_enabled(s)) {
            pci_irq_deassert(pci_dev);
        }
        break;
    case MFI_IQPH:
        //DPRINTF("mmio_write MFI_IQPH\n");
        /* Received high 32 bits of a 64 bit MFI frame address */
        s->frame_hi = val;
        break;
    case MFI_IQPL:
        //DPRINTF("mmio_write MFI_IQPL\n");
        /* Received low 32 bits of a 64 bit MFI frame address */
        /* Fallthrough */
    case MFI_IQP:
        //DPRINTF("mmio_write MFI_IQP\n");
        if (addr == MFI_IQP) {
            /* Received 64 bit MFI frame address */
            s->frame_hi = 0;
        }
        frame_addr = (val & ~0x1F);
        /* Add possible 64 bit offset */
        frame_addr |= ((uint64_t)s->frame_hi << 32);
        s->frame_hi = 0;
        frame_count = (val >> 1) & 0xF;
        megasas_handle_frame(s, frame_addr, frame_count);
        break;
    case MFI_SEQ:
        DPRINTF("mmio_write MFI_SEQ\n");
        /* Magic sequence to start ADP reset */
        if (adp_reset_seq[s->adp_reset] == val) {
            s->adp_reset++;
        } else {
            s->adp_reset = 0;
            s->diag = 0;
        }
        if (s->adp_reset == 6) {
            s->diag = MFI_DIAG_WRITE_ENABLE;
        }
        break;
    case MFI_DIAG:
        DPRINTF("mmio_write MFI_DIAG\n");
        /* ADP reset */
        if ((s->diag & MFI_DIAG_WRITE_ENABLE) &&
            (val & MFI_DIAG_RESET_ADP)) {
            s->diag |= MFI_DIAG_RESET_ADP;
            megasas_soft_reset(s);
            s->adp_reset = 0;
            s->diag = 0;
        }
        break;
    default:
        DPRINTF("mmio_write addr %d!!!\n", addr);
        break;
    }
}

static const MemAreaOps megasas_mmio_ops = {
    .read = megasas_mmio_read,
    .write = megasas_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    }
};

static uint64_t megasas_port_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    return megasas_mmio_read(opaque, addr & 0xff, size);
}

static void megasas_port_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned size)
{
    megasas_mmio_write(opaque, addr & 0xff, val, size);
}

static const MemAreaOps megasas_port_ops = {
    .read = megasas_port_read,
    .write = megasas_port_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static uint64_t megasas_queue_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    return 0;
}

static const MemAreaOps megasas_queue_ops = {
    .read = megasas_queue_read,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    }
};

static void megasas_soft_reset(MegasasState *s)
{
    int i;
    MegasasCmd *cmd;

    for (i = 0; i < s->fw_cmds; i++) {
        cmd = &s->frames[i];
        megasas_abort_command(cmd);
    }
    if (s->fw_state == MFI_FWSTATE_READY) {
        BusChild *kid;

        /*
         * The EFI firmware doesn't handle UA,
         * so we need to clear the Power On/Reset UA
         * after the initial reset.
         */
        QTAILQ_FOREACH(kid, &s->bus.qbus.children, sibling) {
            SCSIDevice *sdev = DO_UPCAST(SCSIDevice, qdev, kid->child);

            sdev->unit_attention = SENSE_CODE(NO_SENSE);
            scsi_device_unit_attention_reported(sdev);
        }
    }
    megasas_reset_frames(s);
    s->reply_queue_len = s->fw_cmds;
    s->reply_queue_pa = 0;
    s->consumer_pa = 0;
    s->producer_pa = 0;
    s->fw_state = MFI_FWSTATE_READY;
    s->doorbell = 0;
    s->intr_mask = MEGASAS_INTR_DISABLED_MASK;
    s->frame_hi = 0;
    s->flags &= ~MEGASAS_MASK_USE_QUEUE64;
    s->event_count++;
    s->boot_event = s->event_count;
}

static void megasas_scsi_reset(DeviceState *dev)
{
    MegasasState *s = MEGASAS(dev);

    megasas_soft_reset(s);
}

static const VMStateDescription vmstate_megasas_gen1 = {
    .name = "megasas",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent, MegasasState),
        //VMSTATE_MSIX(parent, MegasasState),

        VMSTATE_INT32(fw_state, MegasasState),
        VMSTATE_INT32(intr_mask, MegasasState),
        VMSTATE_INT32(doorbell, MegasasState),
        VMSTATE_UINT64(reply_queue_pa, MegasasState),
        VMSTATE_UINT64(consumer_pa, MegasasState),
        VMSTATE_UINT64(producer_pa, MegasasState),

        VMSTATE_UINT32(frame_hi, MegasasState),
        VMSTATE_UINT32(fw_sge, MegasasState),
        VMSTATE_UINT32(fw_cmds, MegasasState),
        VMSTATE_UINT32(flags, MegasasState),
        VMSTATE_INT32(fw_luns, MegasasState),
        VMSTATE_INT32(busy, MegasasState),
        VMSTATE_INT32(diag, MegasasState),
        VMSTATE_INT32(adp_reset, MegasasState),
        VMSTATE_INT32(event_locale, MegasasState),
        VMSTATE_INT32(event_class, MegasasState),
        VMSTATE_INT32(event_count, MegasasState),
        VMSTATE_INT32(shutdown_event, MegasasState),
        VMSTATE_INT32(boot_event, MegasasState),
        VMSTATE_UINT64(sas_addr, MegasasState),
        VMSTATE_INT32(reply_queue_len, MegasasState),
        VMSTATE_INT32(reply_queue_head, MegasasState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_megasas_gen2 = {
    .name = "megasas-gen2",
    .version_id = 0,
    .minimum_version_id = 0,
    .minimum_version_id_old = 0,
    .fields      = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent, MegasasState),
        //VMSTATE_MSIX(parent, MegasasState),

        VMSTATE_INT32(fw_state, MegasasState),
        VMSTATE_INT32(intr_mask, MegasasState),
        VMSTATE_INT32(doorbell, MegasasState),
        VMSTATE_UINT64(reply_queue_pa, MegasasState),
        VMSTATE_UINT64(consumer_pa, MegasasState),
        VMSTATE_UINT64(producer_pa, MegasasState),

        VMSTATE_UINT32(frame_hi, MegasasState),
        VMSTATE_UINT32(fw_sge, MegasasState),
        VMSTATE_UINT32(fw_cmds, MegasasState),
        VMSTATE_UINT32(flags, MegasasState),
        VMSTATE_INT32(fw_luns, MegasasState),
        VMSTATE_INT32(busy, MegasasState),
        VMSTATE_INT32(diag, MegasasState),
        VMSTATE_INT32(adp_reset, MegasasState),
        VMSTATE_INT32(event_locale, MegasasState),
        VMSTATE_INT32(event_class, MegasasState),
        VMSTATE_INT32(event_count, MegasasState),
        VMSTATE_INT32(shutdown_event, MegasasState),
        VMSTATE_INT32(boot_event, MegasasState),
        VMSTATE_UINT64(sas_addr, MegasasState),
        VMSTATE_INT32(reply_queue_len, MegasasState),
        VMSTATE_INT32(reply_queue_head, MegasasState),
        VMSTATE_END_OF_LIST()
    }
};

static void megasas_scsi_uninit(PCIDevice *d)
{
    MegasasState *s = MEGASAS(d);

    /*if (megasas_use_msix(s)) {
        msix_uninit(d, &s->mmio_io, &s->mmio_io);
    }*/
    if (megasas_use_msi(s)) {
    }
}

static const struct SCSIBusInfo megasas_scsi_info = {
    .tcq = true,
    .max_target = MFI_MAX_LD,
    .max_lun = 8,

    .transfer_data = megasas_xfer_complete,
    .get_sg_list = megasas_get_sg_list,
    .complete = megasas_command_complete,
    .cancel = megasas_command_cancel,
};

static int megasas_scsi_init(PCIDevice *dev)
{
    DeviceState *d = DEVICE(dev);
    MegasasState *s = MEGASAS(dev);
    MegasasBaseClass *b = MEGASAS_DEVICE_GET_CLASS(s);
    uint8_t *pci_conf;
    int i, bar_type;
    Error *err = NULL;
    
    s->fw_sge = MEGASAS_DEFAULT_SGE;
    s->fw_cmds = MEGASAS_DEFAULT_FRAMES;
    s->sas_addr = 0;
    s->flags = 0;

    pci_conf = dev->config;

    /* PCI latency timer = 0 */
    pci_conf[PCI_LATENCY_TIMER] = 0;
    /* Interrupt pin 1 */
    pci_conf[PCI_INTERRUPT_PIN] = 0x01;

    memory_area_init_io(&s->mmio_io, VeertuTypeHold(s), &megasas_mmio_ops, s,
                          "megasas-mmio", 0x4000);
    memory_area_init_io(&s->port_io, VeertuTypeHold(s), &megasas_port_ops, s,
                          "megasas-io", 256);
    memory_area_init_io(&s->queue_io, VeertuTypeHold(s), &megasas_queue_ops, s,
                          "megasas-queue", 0x40000);
#if 0
    if (megasas_use_msi(s) &&
        msi_init(dev, 0x50, 1, true, false)) {
        s->flags &= ~MEGASAS_MASK_USE_MSI;
    }
#endif
    /*if (megasas_use_msix(s) &&
        msix_init(dev, 15, &s->mmio_io, b->mmio_bar, 0x2000,
                  &s->mmio_io, b->mmio_bar, 0x3800, 0x68)) {
        s->flags &= ~MEGASAS_MASK_USE_MSIX;
    }*/
    /*if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }*/

    bar_type = PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64;
    pci_register_bar(dev, b->ioport_bar,
                     PCI_BASE_ADDRESS_SPACE_IO, &s->port_io);
    pci_register_bar(dev, b->mmio_bar, bar_type, &s->mmio_io);
    pci_register_bar(dev, 3, bar_type, &s->queue_io);

    /*if (megasas_use_msix(s)) {
        msix_vector_use(dev, 0);
    }*/

    s->fw_state = MFI_FWSTATE_READY;
    if (!s->sas_addr) {
        s->sas_addr = ((NAA_LOCALLY_ASSIGNED_ID << 24) |
                       IEEE_COMPANY_LOCALLY_ASSIGNED) << 36;
        s->sas_addr |= (pci_bus_num(dev->bus) << 16);
        s->sas_addr |= (PCI_SLOT(dev->devfn) << 8);
        s->sas_addr |= PCI_FUNC(dev->devfn);
    }
    if (!s->hba_serial) {
        s->hba_serial = g_strdup(MEGASAS_HBA_SERIAL);
    }
    if (s->fw_sge >= MEGASAS_MAX_SGE - MFI_PASS_FRAME_SIZE) {
        s->fw_sge = MEGASAS_MAX_SGE - MFI_PASS_FRAME_SIZE;
    } else if (s->fw_sge >= 128 - MFI_PASS_FRAME_SIZE) {
        s->fw_sge = 128 - MFI_PASS_FRAME_SIZE;
    } else {
        s->fw_sge = 64 - MFI_PASS_FRAME_SIZE;
    }
    if (s->fw_cmds > MEGASAS_MAX_FRAMES) {
        s->fw_cmds = MEGASAS_MAX_FRAMES;
    }

    if (megasas_is_jbod(s)) {
        s->fw_luns = MFI_MAX_SYS_PDS;
    } else {
        s->fw_luns = MFI_MAX_LD;
    }
    s->producer_pa = 0;
    s->consumer_pa = 0;
    for (i = 0; i < s->fw_cmds; i++) {
        s->frames[i].index = i;
        s->frames[i].context = -1;
        s->frames[i].pa = 0;
        s->frames[i].state = s;
    }

    scsi_bus_new(&s->bus, sizeof(s->bus), DEVICE(dev),
                 &megasas_scsi_info, NULL);
    if (!d->hotplugged) {
        scsi_bus_legacy_handle_cmdline(&s->bus, &err);
        if (err != NULL) {
            error_free(err);
            return -1;
        }
    }
    return 0;
}

static void
megasas_write_config(PCIDevice *pci, uint32_t addr, uint32_t val, int len)
{
    pci_default_write_config(pci, addr, val, len);
}


//not used gen2


typedef struct MegasasInfo {
    const char *name;
    const char *desc;
    const char *product_name;
    const char *product_version;
    uint16_t device_id;
    uint16_t subsystem_id;
    int ioport_bar;
    int mmio_bar;
    bool is_express;
    int osts;
    const VMStateDescription *vmsd;
    Property *props;
} MegasasInfo;

static struct MegasasInfo megasas_devices[] = {
    {
        .name = TYPE_MEGASAS_GEN1,
        .desc = "LSI MegaRAID SAS 1078",
        .product_name = "LSI MegaRAID SAS 8708EM2",
        .product_version = MEGASAS_VERSION_GEN1,
        .device_id = PCI_DEVICE_ID_LSI_SAS1078,
        .subsystem_id = 0x1013,
        .ioport_bar = 2,
        .mmio_bar = 0,
        .osts = MFI_1078_RM | 1,
        .is_express = false,
        .vmsd = &vmstate_megasas_gen1,
    },{
        .name = TYPE_MEGASAS_GEN2,
        .desc = "LSI MegaRAID SAS 2108",
        .product_name = "LSI MegaRAID SAS 9260-8i",
        .product_version = MEGASAS_VERSION_GEN2,
        .device_id = PCI_DEVICE_ID_LSI_SAS0079,
        .subsystem_id = 0x9261,
        .ioport_bar = 0,
        .mmio_bar = 1,
        .osts = MFI_GEN2_RM,
        .is_express = true,
        .vmsd = &vmstate_megasas_gen2,
    }
};

static void megasas_class_init(VeertuTypeClassHold *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);
    MegasasBaseClass *e = MEGASAS_DEVICE_CLASS(oc);
    const MegasasInfo *info = data;

    pc->init = megasas_scsi_init;
    pc->exit = megasas_scsi_uninit;
    pc->vendor_id = PCI_VENDOR_ID_LSI_LOGIC;
    pc->device_id = info->device_id;
    pc->subsystem_vendor_id = PCI_VENDOR_ID_LSI_LOGIC;
    pc->subsystem_id = info->subsystem_id;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->is_express = info->is_express;
    e->mmio_bar = info->mmio_bar;
    e->ioport_bar = info->ioport_bar;
    e->osts = info->osts;
    e->product_name = info->product_name;
    e->product_version = info->product_version;
    dc->reset = megasas_scsi_reset;
    dc->vmsd = info->vmsd;
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
    dc->desc = info->desc;
    pc->config_write = megasas_write_config;
}

static const VeertuTypeInfo megasas_info = {
    .name  = TYPE_MEGASAS_BASE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MegasasState),
    .class_size = sizeof(MegasasBaseClass),
};

void megasas_register_types(void)
{
    int i;

    register_type_internal(&megasas_info);
    for (i = 0; i < ARRAY_SIZE(megasas_devices); i++) {
        const MegasasInfo *info = &megasas_devices[i];
        VeertuTypeInfo type_info = {};

        type_info.name = info->name;
        type_info.parent = TYPE_MEGASAS_BASE;
        type_info.class_data = (void *)info;
        type_info.class_init = megasas_class_init;

        register_type_internal(&type_info);
    }
}
