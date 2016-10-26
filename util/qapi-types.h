/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * schema-defined QAPI types
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#ifndef QAPI_TYPES_H
#define QAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>


#ifndef QAPI_TYPES_BUILTIN_STRUCT_DECL_H
#define QAPI_TYPES_BUILTIN_STRUCT_DECL_H


typedef struct strList
{
    union {
        char *value;
        uint64_t padding;
    };
    struct strList *next;
} strList;

typedef struct intList
{
    union {
        int64_t value;
        uint64_t padding;
    };
    struct intList *next;
} intList;

typedef struct numberList
{
    union {
        double value;
        uint64_t padding;
    };
    struct numberList *next;
} numberList;

typedef struct boolList
{
    union {
        bool value;
        uint64_t padding;
    };
    struct boolList *next;
} boolList;

typedef struct int8List
{
    union {
        int8_t value;
        uint64_t padding;
    };
    struct int8List *next;
} int8List;

typedef struct int16List
{
    union {
        int16_t value;
        uint64_t padding;
    };
    struct int16List *next;
} int16List;

typedef struct int32List
{
    union {
        int32_t value;
        uint64_t padding;
    };
    struct int32List *next;
} int32List;

typedef struct int64List
{
    union {
        int64_t value;
        uint64_t padding;
    };
    struct int64List *next;
} int64List;

typedef struct uint8List
{
    union {
        uint8_t value;
        uint64_t padding;
    };
    struct uint8List *next;
} uint8List;

typedef struct uint16List
{
    union {
        uint16_t value;
        uint64_t padding;
    };
    struct uint16List *next;
} uint16List;

typedef struct uint32List
{
    union {
        uint32_t value;
        uint64_t padding;
    };
    struct uint32List *next;
} uint32List;

typedef struct uint64List
{
    union {
        uint64_t value;
        uint64_t padding;
    };
    struct uint64List *next;
} uint64List;

#endif /* QAPI_TYPES_BUILTIN_STRUCT_DECL_H */


extern const char *ErrorClass_lookup[];
typedef enum ErrorClass
{
    ERROR_CLASS_GENERIC_ERROR = 0,
    ERROR_CLASS_COMMAND_NOT_FOUND = 1,
    ERROR_CLASS_DEVICE_ENCRYPTED = 2,
    ERROR_CLASS_DEVICE_NOT_ACTIVE = 3,
    ERROR_CLASS_DEVICE_NOT_FOUND = 4,
    ERROR_CLASS_KVM_MISSING_CAP = 5,
    ERROR_CLASS_MAX = 6,
} ErrorClass;

typedef struct ErrorClassList
{
    union {
        ErrorClass value;
        uint64_t padding;
    };
    struct ErrorClassList *next;
} ErrorClassList;


typedef struct VersionInfo VersionInfo;

typedef struct VersionInfoList
{
    union {
        VersionInfo *value;
        uint64_t padding;
    };
    struct VersionInfoList *next;
} VersionInfoList;


typedef struct CommandInfo CommandInfo;

typedef struct CommandInfoList
{
    union {
        CommandInfo *value;
        uint64_t padding;
    };
    struct CommandInfoList *next;
} CommandInfoList;

extern const char *OnOffAuto_lookup[];
typedef enum OnOffAuto
{
    ON_OFF_AUTO_AUTO = 0,
    ON_OFF_AUTO_ON = 1,
    ON_OFF_AUTO_OFF = 2,
    ON_OFF_AUTO_MAX = 3,
} OnOffAuto;

typedef struct OnOffAutoList
{
    union {
        OnOffAuto value;
        uint64_t padding;
    };
    struct OnOffAutoList *next;
} OnOffAutoList;


typedef struct SnapshotInfo SnapshotInfo;

typedef struct SnapshotInfoList
{
    union {
        SnapshotInfo *value;
        uint64_t padding;
    };
    struct SnapshotInfoList *next;
} SnapshotInfoList;


typedef struct ImageInfoSpecificQCow2 ImageInfoSpecificQCow2;

typedef struct ImageInfoSpecificQCow2List
{
    union {
        ImageInfoSpecificQCow2 *value;
        uint64_t padding;
    };
    struct ImageInfoSpecificQCow2List *next;
} ImageInfoSpecificQCow2List;


typedef struct ImageInfoSpecificVmdk ImageInfoSpecificVmdk;

typedef struct ImageInfoSpecificVmdkList
{
    union {
        ImageInfoSpecificVmdk *value;
        uint64_t padding;
    };
    struct ImageInfoSpecificVmdkList *next;
} ImageInfoSpecificVmdkList;


typedef struct ImageInfoSpecific ImageInfoSpecific;

typedef struct ImageInfoSpecificList
{
    union {
        ImageInfoSpecific *value;
        uint64_t padding;
    };
    struct ImageInfoSpecificList *next;
} ImageInfoSpecificList;

extern const char *ImageInfoSpecificKind_lookup[];
typedef enum ImageInfoSpecificKind
{
    IMAGE_INFO_SPECIFIC_KIND_QCOW2 = 0,
    IMAGE_INFO_SPECIFIC_KIND_VMDK = 1,
    IMAGE_INFO_SPECIFIC_KIND_MAX = 2,
} ImageInfoSpecificKind;


typedef struct ImageInfo ImageInfo;

typedef struct ImageInfoList
{
    union {
        ImageInfo *value;
        uint64_t padding;
    };
    struct ImageInfoList *next;
} ImageInfoList;


typedef struct ImageCheck ImageCheck;

typedef struct ImageCheckList
{
    union {
        ImageCheck *value;
        uint64_t padding;
    };
    struct ImageCheckList *next;
} ImageCheckList;


typedef struct BlockdevCacheInfo BlockdevCacheInfo;

typedef struct BlockdevCacheInfoList
{
    union {
        BlockdevCacheInfo *value;
        uint64_t padding;
    };
    struct BlockdevCacheInfoList *next;
} BlockdevCacheInfoList;


typedef struct BlockDeviceInfo BlockDeviceInfo;

typedef struct BlockDeviceInfoList
{
    union {
        BlockDeviceInfo *value;
        uint64_t padding;
    };
    struct BlockDeviceInfoList *next;
} BlockDeviceInfoList;

extern const char *BlockDeviceIoStatus_lookup[];
typedef enum BlockDeviceIoStatus
{
    BLOCK_DEVICE_IO_STATUS_OK = 0,
    BLOCK_DEVICE_IO_STATUS_FAILED = 1,
    BLOCK_DEVICE_IO_STATUS_NOSPACE = 2,
    BLOCK_DEVICE_IO_STATUS_MAX = 3,
} BlockDeviceIoStatus;

typedef struct BlockDeviceIoStatusList
{
    union {
        BlockDeviceIoStatus value;
        uint64_t padding;
    };
    struct BlockDeviceIoStatusList *next;
} BlockDeviceIoStatusList;


typedef struct BlockDeviceMapEntry BlockDeviceMapEntry;

typedef struct BlockDeviceMapEntryList
{
    union {
        BlockDeviceMapEntry *value;
        uint64_t padding;
    };
    struct BlockDeviceMapEntryList *next;
} BlockDeviceMapEntryList;


typedef struct BlockDirtyInfo BlockDirtyInfo;

typedef struct BlockDirtyInfoList
{
    union {
        BlockDirtyInfo *value;
        uint64_t padding;
    };
    struct BlockDirtyInfoList *next;
} BlockDirtyInfoList;


typedef struct BlockInfo BlockInfo;

typedef struct BlockInfoList
{
    union {
        BlockInfo *value;
        uint64_t padding;
    };
    struct BlockInfoList *next;
} BlockInfoList;


typedef struct BlockDeviceStats BlockDeviceStats;

typedef struct BlockDeviceStatsList
{
    union {
        BlockDeviceStats *value;
        uint64_t padding;
    };
    struct BlockDeviceStatsList *next;
} BlockDeviceStatsList;


typedef struct BlockStats BlockStats;

typedef struct BlockStatsList
{
    union {
        BlockStats *value;
        uint64_t padding;
    };
    struct BlockStatsList *next;
} BlockStatsList;

extern const char *BlockdevOnError_lookup[];
typedef enum BlockdevOnError
{
    BLOCKDEV_ON_ERROR_REPORT = 0,
    BLOCKDEV_ON_ERROR_IGNORE = 1,
    BLOCKDEV_ON_ERROR_ENOSPC = 2,
    BLOCKDEV_ON_ERROR_STOP = 3,
    BLOCKDEV_ON_ERROR_MAX = 4,
} BlockdevOnError;

typedef struct BlockdevOnErrorList
{
    union {
        BlockdevOnError value;
        uint64_t padding;
    };
    struct BlockdevOnErrorList *next;
} BlockdevOnErrorList;

extern const char *MirrorSyncMode_lookup[];
typedef enum MirrorSyncMode
{
    MIRROR_SYNC_MODE_TOP = 0,
    MIRROR_SYNC_MODE_FULL = 1,
    MIRROR_SYNC_MODE_NONE = 2,
    MIRROR_SYNC_MODE_MAX = 3,
} MirrorSyncMode;

typedef struct MirrorSyncModeList
{
    union {
        MirrorSyncMode value;
        uint64_t padding;
    };
    struct MirrorSyncModeList *next;
} MirrorSyncModeList;

extern const char *BlockJobType_lookup[];
typedef enum BlockJobType
{
    BLOCK_JOB_TYPE_COMMIT = 0,
    BLOCK_JOB_TYPE_STREAM = 1,
    BLOCK_JOB_TYPE_MIRROR = 2,
    BLOCK_JOB_TYPE_BACKUP = 3,
    BLOCK_JOB_TYPE_MAX = 4,
} BlockJobType;

typedef struct BlockJobTypeList
{
    union {
        BlockJobType value;
        uint64_t padding;
    };
    struct BlockJobTypeList *next;
} BlockJobTypeList;


typedef struct BlockJobInfo BlockJobInfo;

typedef struct BlockJobInfoList
{
    union {
        BlockJobInfo *value;
        uint64_t padding;
    };
    struct BlockJobInfoList *next;
} BlockJobInfoList;

extern const char *NewImageMode_lookup[];
typedef enum NewImageMode
{
    NEW_IMAGE_MODE_EXISTING = 0,
    NEW_IMAGE_MODE_ABSOLUTE_PATHS = 1,
    NEW_IMAGE_MODE_MAX = 2,
} NewImageMode;

typedef struct NewImageModeList
{
    union {
        NewImageMode value;
        uint64_t padding;
    };
    struct NewImageModeList *next;
} NewImageModeList;


typedef struct BlockdevSnapshot BlockdevSnapshot;

typedef struct BlockdevSnapshotList
{
    union {
        BlockdevSnapshot *value;
        uint64_t padding;
    };
    struct BlockdevSnapshotList *next;
} BlockdevSnapshotList;


typedef struct DriveBackup DriveBackup;

typedef struct DriveBackupList
{
    union {
        DriveBackup *value;
        uint64_t padding;
    };
    struct DriveBackupList *next;
} DriveBackupList;


typedef struct BlockdevBackup BlockdevBackup;

typedef struct BlockdevBackupList
{
    union {
        BlockdevBackup *value;
        uint64_t padding;
    };
    struct BlockdevBackupList *next;
} BlockdevBackupList;

extern const char *BlockdevDiscardOptions_lookup[];
typedef enum BlockdevDiscardOptions
{
    BLOCKDEV_DISCARD_OPTIONS_IGNORE = 0,
    BLOCKDEV_DISCARD_OPTIONS_UNMAP = 1,
    BLOCKDEV_DISCARD_OPTIONS_MAX = 2,
} BlockdevDiscardOptions;

typedef struct BlockdevDiscardOptionsList
{
    union {
        BlockdevDiscardOptions value;
        uint64_t padding;
    };
    struct BlockdevDiscardOptionsList *next;
} BlockdevDiscardOptionsList;

extern const char *BlockdevDetectZeroesOptions_lookup[];
typedef enum BlockdevDetectZeroesOptions
{
    BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF = 0,
    BLOCKDEV_DETECT_ZEROES_OPTIONS_ON = 1,
    BLOCKDEV_DETECT_ZEROES_OPTIONS_UNMAP = 2,
    BLOCKDEV_DETECT_ZEROES_OPTIONS_MAX = 3,
} BlockdevDetectZeroesOptions;

typedef struct BlockdevDetectZeroesOptionsList
{
    union {
        BlockdevDetectZeroesOptions value;
        uint64_t padding;
    };
    struct BlockdevDetectZeroesOptionsList *next;
} BlockdevDetectZeroesOptionsList;

extern const char *BlockdevAioOptions_lookup[];
typedef enum BlockdevAioOptions
{
    BLOCKDEV_AIO_OPTIONS_THREADS = 0,
    BLOCKDEV_AIO_OPTIONS_NATIVE = 1,
    BLOCKDEV_AIO_OPTIONS_MAX = 2,
} BlockdevAioOptions;

typedef struct BlockdevAioOptionsList
{
    union {
        BlockdevAioOptions value;
        uint64_t padding;
    };
    struct BlockdevAioOptionsList *next;
} BlockdevAioOptionsList;


typedef struct BlockdevCacheOptions BlockdevCacheOptions;

typedef struct BlockdevCacheOptionsList
{
    union {
        BlockdevCacheOptions *value;
        uint64_t padding;
    };
    struct BlockdevCacheOptionsList *next;
} BlockdevCacheOptionsList;

extern const char *BlockdevDriver_lookup[];
typedef enum BlockdevDriver
{
    BLOCKDEV_DRIVER_ARCHIPELAGO = 0,
    BLOCKDEV_DRIVER_BLKDEBUG = 1,
    BLOCKDEV_DRIVER_BLKVERIFY = 2,
    BLOCKDEV_DRIVER_BOCHS = 3,
    BLOCKDEV_DRIVER_CLOOP = 4,
    BLOCKDEV_DRIVER_DMG = 5,
    BLOCKDEV_DRIVER_FILE = 6,
    BLOCKDEV_DRIVER_FTP = 7,
    BLOCKDEV_DRIVER_FTPS = 8,
    BLOCKDEV_DRIVER_HOST_CDROM = 9,
    BLOCKDEV_DRIVER_HOST_DEVICE = 10,
    BLOCKDEV_DRIVER_HOST_FLOPPY = 11,
    BLOCKDEV_DRIVER_HTTP = 12,
    BLOCKDEV_DRIVER_HTTPS = 13,
    BLOCKDEV_DRIVER_NULL_AIO = 14,
    BLOCKDEV_DRIVER_NULL_CO = 15,
    BLOCKDEV_DRIVER_PARALLELS = 16,
    BLOCKDEV_DRIVER_QCOW = 17,
    BLOCKDEV_DRIVER_QCOW2 = 18,
    BLOCKDEV_DRIVER_QED = 19,
    BLOCKDEV_DRIVER_QUORUM = 20,
    BLOCKDEV_DRIVER_RAW = 21,
    BLOCKDEV_DRIVER_TFTP = 22,
    BLOCKDEV_DRIVER_VDI = 23,
    BLOCKDEV_DRIVER_VHDX = 24,
    BLOCKDEV_DRIVER_VMDK = 25,
    BLOCKDEV_DRIVER_VPC = 26,
    BLOCKDEV_DRIVER_VVFAT = 27,
    BLOCKDEV_DRIVER_MAX = 28,
} BlockdevDriver;

typedef struct BlockdevDriverList
{
    union {
        BlockdevDriver value;
        uint64_t padding;
    };
    struct BlockdevDriverList *next;
} BlockdevDriverList;


typedef struct BlockdevOptionsBase BlockdevOptionsBase;

typedef struct BlockdevOptionsBaseList
{
    union {
        BlockdevOptionsBase *value;
        uint64_t padding;
    };
    struct BlockdevOptionsBaseList *next;
} BlockdevOptionsBaseList;


typedef struct BlockdevOptionsFile BlockdevOptionsFile;

typedef struct BlockdevOptionsFileList
{
    union {
        BlockdevOptionsFile *value;
        uint64_t padding;
    };
    struct BlockdevOptionsFileList *next;
} BlockdevOptionsFileList;


typedef struct BlockdevOptionsNull BlockdevOptionsNull;

typedef struct BlockdevOptionsNullList
{
    union {
        BlockdevOptionsNull *value;
        uint64_t padding;
    };
    struct BlockdevOptionsNullList *next;
} BlockdevOptionsNullList;


typedef struct BlockdevOptionsVVFAT BlockdevOptionsVVFAT;

typedef struct BlockdevOptionsVVFATList
{
    union {
        BlockdevOptionsVVFAT *value;
        uint64_t padding;
    };
    struct BlockdevOptionsVVFATList *next;
} BlockdevOptionsVVFATList;


typedef struct BlockdevOptionsGenericFormat BlockdevOptionsGenericFormat;

typedef struct BlockdevOptionsGenericFormatList
{
    union {
        BlockdevOptionsGenericFormat *value;
        uint64_t padding;
    };
    struct BlockdevOptionsGenericFormatList *next;
} BlockdevOptionsGenericFormatList;


typedef struct BlockdevOptionsGenericCOWFormat BlockdevOptionsGenericCOWFormat;

typedef struct BlockdevOptionsGenericCOWFormatList
{
    union {
        BlockdevOptionsGenericCOWFormat *value;
        uint64_t padding;
    };
    struct BlockdevOptionsGenericCOWFormatList *next;
} BlockdevOptionsGenericCOWFormatList;

extern const char *Qcow2OverlapCheckMode_lookup[];
typedef enum Qcow2OverlapCheckMode
{
    QCOW2_OVERLAP_CHECK_MODE_NONE = 0,
    QCOW2_OVERLAP_CHECK_MODE_CONSTANT = 1,
    QCOW2_OVERLAP_CHECK_MODE_CACHED = 2,
    QCOW2_OVERLAP_CHECK_MODE_ALL = 3,
    QCOW2_OVERLAP_CHECK_MODE_MAX = 4,
} Qcow2OverlapCheckMode;

typedef struct Qcow2OverlapCheckModeList
{
    union {
        Qcow2OverlapCheckMode value;
        uint64_t padding;
    };
    struct Qcow2OverlapCheckModeList *next;
} Qcow2OverlapCheckModeList;


typedef struct Qcow2OverlapCheckFlags Qcow2OverlapCheckFlags;

typedef struct Qcow2OverlapCheckFlagsList
{
    union {
        Qcow2OverlapCheckFlags *value;
        uint64_t padding;
    };
    struct Qcow2OverlapCheckFlagsList *next;
} Qcow2OverlapCheckFlagsList;


typedef struct Qcow2OverlapChecks Qcow2OverlapChecks;

typedef struct Qcow2OverlapChecksList
{
    union {
        Qcow2OverlapChecks *value;
        uint64_t padding;
    };
    struct Qcow2OverlapChecksList *next;
} Qcow2OverlapChecksList;

extern const char *Qcow2OverlapChecksKind_lookup[];
typedef enum Qcow2OverlapChecksKind
{
    QCOW2_OVERLAP_CHECKS_KIND_FLAGS = 0,
    QCOW2_OVERLAP_CHECKS_KIND_MODE = 1,
    QCOW2_OVERLAP_CHECKS_KIND_MAX = 2,
} Qcow2OverlapChecksKind;


typedef struct BlockdevOptionsQcow2 BlockdevOptionsQcow2;

typedef struct BlockdevOptionsQcow2List
{
    union {
        BlockdevOptionsQcow2 *value;
        uint64_t padding;
    };
    struct BlockdevOptionsQcow2List *next;
} BlockdevOptionsQcow2List;


typedef struct BlockdevOptionsArchipelago BlockdevOptionsArchipelago;

typedef struct BlockdevOptionsArchipelagoList
{
    union {
        BlockdevOptionsArchipelago *value;
        uint64_t padding;
    };
    struct BlockdevOptionsArchipelagoList *next;
} BlockdevOptionsArchipelagoList;

extern const char *BlkdebugEvent_lookup[];
typedef enum BlkdebugEvent
{
    BLKDEBUG_EVENT_L1_UPDATE = 0,
    BLKDEBUG_EVENT_L1_GROW_ALLOC_TABLE = 1,
    BLKDEBUG_EVENT_L1_GROW_WRITE_TABLE = 2,
    BLKDEBUG_EVENT_L1_GROW_ACTIVATE_TABLE = 3,
    BLKDEBUG_EVENT_L2_LOAD = 4,
    BLKDEBUG_EVENT_L2_UPDATE = 5,
    BLKDEBUG_EVENT_L2_UPDATE_COMPRESSED = 6,
    BLKDEBUG_EVENT_L2_ALLOC_COW_READ = 7,
    BLKDEBUG_EVENT_L2_ALLOC_WRITE = 8,
    BLKDEBUG_EVENT_READ_AIO = 9,
    BLKDEBUG_EVENT_READ_BACKING_AIO = 10,
    BLKDEBUG_EVENT_READ_COMPRESSED = 11,
    BLKDEBUG_EVENT_WRITE_AIO = 12,
    BLKDEBUG_EVENT_WRITE_COMPRESSED = 13,
    BLKDEBUG_EVENT_VMSTATE_LOAD = 14,
    BLKDEBUG_EVENT_VMSTATE_SAVE = 15,
    BLKDEBUG_EVENT_COW_READ = 16,
    BLKDEBUG_EVENT_COW_WRITE = 17,
    BLKDEBUG_EVENT_REFTABLE_LOAD = 18,
    BLKDEBUG_EVENT_REFTABLE_GROW = 19,
    BLKDEBUG_EVENT_REFTABLE_UPDATE = 20,
    BLKDEBUG_EVENT_REFBLOCK_LOAD = 21,
    BLKDEBUG_EVENT_REFBLOCK_UPDATE = 22,
    BLKDEBUG_EVENT_REFBLOCK_UPDATE_PART = 23,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC = 24,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC_HOOKUP = 25,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC_WRITE = 26,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC_WRITE_BLOCKS = 27,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC_WRITE_TABLE = 28,
    BLKDEBUG_EVENT_REFBLOCK_ALLOC_SWITCH_TABLE = 29,
    BLKDEBUG_EVENT_CLUSTER_ALLOC = 30,
    BLKDEBUG_EVENT_CLUSTER_ALLOC_BYTES = 31,
    BLKDEBUG_EVENT_CLUSTER_FREE = 32,
    BLKDEBUG_EVENT_FLUSH_TO_OS = 33,
    BLKDEBUG_EVENT_FLUSH_TO_DISK = 34,
    BLKDEBUG_EVENT_PWRITEV_RMW_HEAD = 35,
    BLKDEBUG_EVENT_PWRITEV_RMW_AFTER_HEAD = 36,
    BLKDEBUG_EVENT_PWRITEV_RMW_TAIL = 37,
    BLKDEBUG_EVENT_PWRITEV_RMW_AFTER_TAIL = 38,
    BLKDEBUG_EVENT_PWRITEV = 39,
    BLKDEBUG_EVENT_PWRITEV_ZERO = 40,
    BLKDEBUG_EVENT_PWRITEV_DONE = 41,
    BLKDEBUG_EVENT_EMPTY_IMAGE_PREPARE = 42,
    BLKDEBUG_EVENT_MAX = 43,
} BlkdebugEvent;

typedef struct BlkdebugEventList
{
    union {
        BlkdebugEvent value;
        uint64_t padding;
    };
    struct BlkdebugEventList *next;
} BlkdebugEventList;


typedef struct BlkdebugInjectErrorOptions BlkdebugInjectErrorOptions;

typedef struct BlkdebugInjectErrorOptionsList
{
    union {
        BlkdebugInjectErrorOptions *value;
        uint64_t padding;
    };
    struct BlkdebugInjectErrorOptionsList *next;
} BlkdebugInjectErrorOptionsList;


typedef struct BlkdebugSetStateOptions BlkdebugSetStateOptions;

typedef struct BlkdebugSetStateOptionsList
{
    union {
        BlkdebugSetStateOptions *value;
        uint64_t padding;
    };
    struct BlkdebugSetStateOptionsList *next;
} BlkdebugSetStateOptionsList;


typedef struct BlockdevOptionsBlkdebug BlockdevOptionsBlkdebug;

typedef struct BlockdevOptionsBlkdebugList
{
    union {
        BlockdevOptionsBlkdebug *value;
        uint64_t padding;
    };
    struct BlockdevOptionsBlkdebugList *next;
} BlockdevOptionsBlkdebugList;


typedef struct BlockdevOptionsBlkverify BlockdevOptionsBlkverify;

typedef struct BlockdevOptionsBlkverifyList
{
    union {
        BlockdevOptionsBlkverify *value;
        uint64_t padding;
    };
    struct BlockdevOptionsBlkverifyList *next;
} BlockdevOptionsBlkverifyList;

extern const char *QuorumReadPattern_lookup[];
typedef enum QuorumReadPattern
{
    QUORUM_READ_PATTERN_QUORUM = 0,
    QUORUM_READ_PATTERN_FIFO = 1,
    QUORUM_READ_PATTERN_MAX = 2,
} QuorumReadPattern;

typedef struct QuorumReadPatternList
{
    union {
        QuorumReadPattern value;
        uint64_t padding;
    };
    struct QuorumReadPatternList *next;
} QuorumReadPatternList;


typedef struct BlockdevOptionsQuorum BlockdevOptionsQuorum;

typedef struct BlockdevOptionsQuorumList
{
    union {
        BlockdevOptionsQuorum *value;
        uint64_t padding;
    };
    struct BlockdevOptionsQuorumList *next;
} BlockdevOptionsQuorumList;


typedef struct BlockdevOptions BlockdevOptions;

typedef struct BlockdevOptionsList
{
    union {
        BlockdevOptions *value;
        uint64_t padding;
    };
    struct BlockdevOptionsList *next;
} BlockdevOptionsList;



typedef struct BlockdevRef BlockdevRef;

typedef struct BlockdevRefList
{
    union {
        BlockdevRef *value;
        uint64_t padding;
    };
    struct BlockdevRefList *next;
} BlockdevRefList;

extern const char *BlockdevRefKind_lookup[];
typedef enum BlockdevRefKind
{
    BLOCKDEV_REF_KIND_DEFINITION = 0,
    BLOCKDEV_REF_KIND_REFERENCE = 1,
    BLOCKDEV_REF_KIND_MAX = 2,
} BlockdevRefKind;

extern const char *BlockErrorAction_lookup[];
typedef enum BlockErrorAction
{
    BLOCK_ERROR_ACTION_IGNORE = 0,
    BLOCK_ERROR_ACTION_REPORT = 1,
    BLOCK_ERROR_ACTION_STOP = 2,
    BLOCK_ERROR_ACTION_MAX = 3,
} BlockErrorAction;

typedef struct BlockErrorActionList
{
    union {
        BlockErrorAction value;
        uint64_t padding;
    };
    struct BlockErrorActionList *next;
} BlockErrorActionList;

extern const char *PreallocMode_lookup[];
typedef enum PreallocMode
{
    PREALLOC_MODE_OFF = 0,
    PREALLOC_MODE_METADATA = 1,
    PREALLOC_MODE_FALLOC = 2,
    PREALLOC_MODE_FULL = 3,
    PREALLOC_MODE_MAX = 4,
} PreallocMode;

typedef struct PreallocModeList
{
    union {
        PreallocMode value;
        uint64_t padding;
    };
    struct PreallocModeList *next;
} PreallocModeList;

extern const char *BiosAtaTranslation_lookup[];
typedef enum BiosAtaTranslation
{
    BIOS_ATA_TRANSLATION_AUTO = 0,
    BIOS_ATA_TRANSLATION_NONE = 1,
    BIOS_ATA_TRANSLATION_LBA = 2,
    BIOS_ATA_TRANSLATION_LARGE = 3,
    BIOS_ATA_TRANSLATION_RECHS = 4,
    BIOS_ATA_TRANSLATION_MAX = 5,
} BiosAtaTranslation;

typedef struct BiosAtaTranslationList
{
    union {
        BiosAtaTranslation value;
        uint64_t padding;
    };
    struct BiosAtaTranslationList *next;
} BiosAtaTranslationList;


typedef struct BlockdevSnapshotInternal BlockdevSnapshotInternal;

typedef struct BlockdevSnapshotInternalList
{
    union {
        BlockdevSnapshotInternal *value;
        uint64_t padding;
    };
    struct BlockdevSnapshotInternalList *next;
} BlockdevSnapshotInternalList;

extern const char *TraceEventState_lookup[];
typedef enum TraceEventState
{
    TRACE_EVENT_STATE_UNAVAILABLE = 0,
    TRACE_EVENT_STATE_DISABLED = 1,
    TRACE_EVENT_STATE_ENABLED = 2,
    TRACE_EVENT_STATE_MAX = 3,
} TraceEventState;

typedef struct TraceEventStateList
{
    union {
        TraceEventState value;
        uint64_t padding;
    };
    struct TraceEventStateList *next;
} TraceEventStateList;


typedef struct TraceEventInfo TraceEventInfo;

typedef struct TraceEventInfoList
{
    union {
        TraceEventInfo *value;
        uint64_t padding;
    };
    struct TraceEventInfoList *next;
} TraceEventInfoList;

extern const char *LostTickPolicy_lookup[];
typedef enum LostTickPolicy
{
    LOST_TICK_POLICY_DISCARD = 0,
    LOST_TICK_POLICY_DELAY = 1,
    LOST_TICK_POLICY_MERGE = 2,
    LOST_TICK_POLICY_SLEW = 3,
    LOST_TICK_POLICY_MAX = 4,
} LostTickPolicy;

typedef struct LostTickPolicyList
{
    union {
        LostTickPolicy value;
        uint64_t padding;
    };
    struct LostTickPolicyList *next;
} LostTickPolicyList;


typedef struct NameInfo NameInfo;

typedef struct NameInfoList
{
    union {
        NameInfo *value;
        uint64_t padding;
    };
    struct NameInfoList *next;
} NameInfoList;


typedef struct VmxInfo VmxInfo;

typedef struct VmxInfoList
{
    union {
        VmxInfo *value;
        uint64_t padding;
    };
    struct VmxInfoList *next;
} VmxInfoList;

extern const char *RunState_lookup[];
typedef enum RunState
{
    RUN_STATE_DEBUG = 0,
    RUN_STATE_INMIGRATE = 1,
    RUN_STATE_INTERNAL_ERROR = 2,
    RUN_STATE_IO_ERROR = 3,
    RUN_STATE_PAUSED = 4,
    RUN_STATE_POSTMIGRATE = 5,
    RUN_STATE_PRELAUNCH = 6,
    RUN_STATE_FINISH_MIGRATE = 7,
    RUN_STATE_RESTORE_VM = 8,
    RUN_STATE_RUNNING = 9,
    RUN_STATE_SAVE_VM = 10,
    RUN_STATE_SHUTDOWN = 11,
    RUN_STATE_SUSPENDED = 12,
    RUN_STATE_WATCHDOG = 13,
    RUN_STATE_GUEST_PANICKED = 14,
    RUN_STATE_MAX = 15,
} RunState;

typedef struct RunStateList
{
    union {
        RunState value;
        uint64_t padding;
    };
    struct RunStateList *next;
} RunStateList;


typedef struct StatusInfo StatusInfo;

typedef struct StatusInfoList
{
    union {
        StatusInfo *value;
        uint64_t padding;
    };
    struct StatusInfoList *next;
} StatusInfoList;


typedef struct UuidInfo UuidInfo;

typedef struct UuidInfoList
{
    union {
        UuidInfo *value;
        uint64_t padding;
    };
    struct UuidInfoList *next;
} UuidInfoList;


typedef struct ChardevInfo ChardevInfo;

typedef struct ChardevInfoList
{
    union {
        ChardevInfo *value;
        uint64_t padding;
    };
    struct ChardevInfoList *next;
} ChardevInfoList;


typedef struct ChardevBackendInfo ChardevBackendInfo;

typedef struct ChardevBackendInfoList
{
    union {
        ChardevBackendInfo *value;
        uint64_t padding;
    };
    struct ChardevBackendInfoList *next;
} ChardevBackendInfoList;

extern const char *DataFormat_lookup[];
typedef enum DataFormat
{
    DATA_FORMAT_UTF8 = 0,
    DATA_FORMAT_BASE64 = 1,
    DATA_FORMAT_MAX = 2,
} DataFormat;

typedef struct DataFormatList
{
    union {
        DataFormat value;
        uint64_t padding;
    };
    struct DataFormatList *next;
} DataFormatList;


typedef struct EventInfo EventInfo;

typedef struct EventInfoList
{
    union {
        EventInfo *value;
        uint64_t padding;
    };
    struct EventInfoList *next;
} EventInfoList;


typedef struct MigrationStats MigrationStats;

typedef struct MigrationStatsList
{
    union {
        MigrationStats *value;
        uint64_t padding;
    };
    struct MigrationStatsList *next;
} MigrationStatsList;


typedef struct XBZRLECacheStats XBZRLECacheStats;

typedef struct XBZRLECacheStatsList
{
    union {
        XBZRLECacheStats *value;
        uint64_t padding;
    };
    struct XBZRLECacheStatsList *next;
} XBZRLECacheStatsList;


typedef struct MigrationInfo MigrationInfo;

typedef struct MigrationInfoList
{
    union {
        MigrationInfo *value;
        uint64_t padding;
    };
    struct MigrationInfoList *next;
} MigrationInfoList;

extern const char *MigrationCapability_lookup[];
typedef enum MigrationCapability
{
    MIGRATION_CAPABILITY_XBZRLE = 0,
    MIGRATION_CAPABILITY_RDMA_PIN_ALL = 1,
    MIGRATION_CAPABILITY_AUTO_CONVERGE = 2,
    MIGRATION_CAPABILITY_ZERO_BLOCKS = 3,
    MIGRATION_CAPABILITY_MAX = 4,
} MigrationCapability;

typedef struct MigrationCapabilityList
{
    union {
        MigrationCapability value;
        uint64_t padding;
    };
    struct MigrationCapabilityList *next;
} MigrationCapabilityList;


typedef struct MigrationCapabilityStatus MigrationCapabilityStatus;

typedef struct MigrationCapabilityStatusList
{
    union {
        MigrationCapabilityStatus *value;
        uint64_t padding;
    };
    struct MigrationCapabilityStatusList *next;
} MigrationCapabilityStatusList;


typedef struct MouseInfo MouseInfo;

typedef struct MouseInfoList
{
    union {
        MouseInfo *value;
        uint64_t padding;
    };
    struct MouseInfoList *next;
} MouseInfoList;


typedef struct CpuInfo CpuInfo;

typedef struct CpuInfoList
{
    union {
        CpuInfo *value;
        uint64_t padding;
    };
    struct CpuInfoList *next;
} CpuInfoList;


typedef struct IOThreadInfo IOThreadInfo;

typedef struct IOThreadInfoList
{
    union {
        IOThreadInfo *value;
        uint64_t padding;
    };
    struct IOThreadInfoList *next;
} IOThreadInfoList;

extern const char *NetworkAddressFamily_lookup[];
typedef enum NetworkAddressFamily
{
    NETWORK_ADDRESS_FAMILY_IPV4 = 0,
    NETWORK_ADDRESS_FAMILY_IPV6 = 1,
    NETWORK_ADDRESS_FAMILY_UNIX = 2,
    NETWORK_ADDRESS_FAMILY_UNKNOWN = 3,
    NETWORK_ADDRESS_FAMILY_MAX = 4,
} NetworkAddressFamily;

typedef struct NetworkAddressFamilyList
{
    union {
        NetworkAddressFamily value;
        uint64_t padding;
    };
    struct NetworkAddressFamilyList *next;
} NetworkAddressFamilyList;

typedef struct SpiceBasicInfo SpiceBasicInfo;

typedef struct SpiceBasicInfoList
{
    union {
        SpiceBasicInfo *value;
        uint64_t padding;
    };
    struct SpiceBasicInfoList *next;
} SpiceBasicInfoList;


typedef struct SpiceServerInfo SpiceServerInfo;

typedef struct SpiceServerInfoList
{
    union {
        SpiceServerInfo *value;
        uint64_t padding;
    };
    struct SpiceServerInfoList *next;
} SpiceServerInfoList;


typedef struct SpiceChannel SpiceChannel;

typedef struct SpiceChannelList
{
    union {
        SpiceChannel *value;
        uint64_t padding;
    };
    struct SpiceChannelList *next;
} SpiceChannelList;

extern const char *SpiceQueryMouseMode_lookup[];
typedef enum SpiceQueryMouseMode
{
    SPICE_QUERY_MOUSE_MODE_CLIENT = 0,
    SPICE_QUERY_MOUSE_MODE_SERVER = 1,
    SPICE_QUERY_MOUSE_MODE_UNKNOWN = 2,
    SPICE_QUERY_MOUSE_MODE_MAX = 3,
} SpiceQueryMouseMode;

typedef struct SpiceQueryMouseModeList
{
    union {
        SpiceQueryMouseMode value;
        uint64_t padding;
    };
    struct SpiceQueryMouseModeList *next;
} SpiceQueryMouseModeList;


typedef struct SpiceInfo SpiceInfo;

typedef struct SpiceInfoList
{
    union {
        SpiceInfo *value;
        uint64_t padding;
    };
    struct SpiceInfoList *next;
} SpiceInfoList;


typedef struct BalloonInfo BalloonInfo;

typedef struct BalloonInfoList
{
    union {
        BalloonInfo *value;
        uint64_t padding;
    };
    struct BalloonInfoList *next;
} BalloonInfoList;


typedef struct PciMemoryRange PciMemoryRange;

typedef struct PciMemoryRangeList
{
    union {
        PciMemoryRange *value;
        uint64_t padding;
    };
    struct PciMemoryRangeList *next;
} PciMemoryRangeList;


typedef struct PciMemoryRegion PciMemoryRegion;

typedef struct PciMemoryRegionList
{
    union {
        PciMemoryRegion *value;
        uint64_t padding;
    };
    struct PciMemoryRegionList *next;
} PciMemoryRegionList;


typedef struct PciBridgeInfo PciBridgeInfo;

typedef struct PciBridgeInfoList
{
    union {
        PciBridgeInfo *value;
        uint64_t padding;
    };
    struct PciBridgeInfoList *next;
} PciBridgeInfoList;


typedef struct PciDeviceInfo PciDeviceInfo;

typedef struct PciDeviceInfoList
{
    union {
        PciDeviceInfo *value;
        uint64_t padding;
    };
    struct PciDeviceInfoList *next;
} PciDeviceInfoList;


typedef struct PciInfo PciInfo;

typedef struct PciInfoList
{
    union {
        PciInfo *value;
        uint64_t padding;
    };
    struct PciInfoList *next;
} PciInfoList;


typedef struct Abort Abort;

typedef struct AbortList
{
    union {
        Abort *value;
        uint64_t padding;
    };
    struct AbortList *next;
} AbortList;


typedef struct TransactionAction TransactionAction;

typedef struct TransactionActionList
{
    union {
        TransactionAction *value;
        uint64_t padding;
    };
    struct TransactionActionList *next;
} TransactionActionList;

extern const char *TransactionActionKind_lookup[];
typedef enum TransactionActionKind
{
    TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_SYNC = 0,
    TRANSACTION_ACTION_KIND_DRIVE_BACKUP = 1,
    TRANSACTION_ACTION_KIND_BLOCKDEV_BACKUP = 2,
    TRANSACTION_ACTION_KIND_ABORT = 3,
    TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_INTERNAL_SYNC = 4,
    TRANSACTION_ACTION_KIND_MAX = 5,
} TransactionActionKind;


typedef struct OProInfo OProInfo;

typedef struct ObjectPropertyInfoList
{
    union {
        OProInfo *value;
        uint64_t padding;
    };
    struct ObjectPropertyInfoList *next;
} ObjectPropertyInfoList;


typedef struct OTypeInfo OTypeInfo;

typedef struct ObjectTypeInfoList
{
    union {
        OTypeInfo *value;
        uint64_t padding;
    };
    struct ObjectTypeInfoList *next;
} ObjectTypeInfoList;


typedef struct DevicePropertyInfo DevicePropertyInfo;

typedef struct DevicePropertyInfoList
{
    union {
        DevicePropertyInfo *value;
        uint64_t padding;
    };
    struct DevicePropertyInfoList *next;
} DevicePropertyInfoList;

extern const char *DumpGuestMemoryFormat_lookup[];
typedef enum DumpGuestMemoryFormat
{
    DUMP_GUEST_MEMORY_FORMAT_ELF = 0,
    DUMP_GUEST_MEMORY_FORMAT_KDUMP_ZLIB = 1,
    DUMP_GUEST_MEMORY_FORMAT_KDUMP_LZO = 2,
    DUMP_GUEST_MEMORY_FORMAT_KDUMP_SNAPPY = 3,
    DUMP_GUEST_MEMORY_FORMAT_MAX = 4,
} DumpGuestMemoryFormat;

typedef struct DumpGuestMemoryFormatList
{
    union {
        DumpGuestMemoryFormat value;
        uint64_t padding;
    };
    struct DumpGuestMemoryFormatList *next;
} DumpGuestMemoryFormatList;


typedef struct DumpGuestMemoryCapability DumpGuestMemoryCapability;

typedef struct DumpGuestMemoryCapabilityList
{
    union {
        DumpGuestMemoryCapability *value;
        uint64_t padding;
    };
    struct DumpGuestMemoryCapabilityList *next;
} DumpGuestMemoryCapabilityList;


typedef struct NetdevNoneOptions NetdevNoneOptions;

typedef struct NetdevNoneOptionsList
{
    union {
        NetdevNoneOptions *value;
        uint64_t padding;
    };
    struct NetdevNoneOptionsList *next;
} NetdevNoneOptionsList;


typedef struct NetLegacyNicOptions NetLegacyNicOptions;

typedef struct NetLegacyNicOptionsList
{
    union {
        NetLegacyNicOptions *value;
        uint64_t padding;
    };
    struct NetLegacyNicOptionsList *next;
} NetLegacyNicOptionsList;


typedef struct String String;

typedef struct StringList
{
    union {
        String *value;
        uint64_t padding;
    };
    struct StringList *next;
} StringList;


typedef struct NetdevUserOptions NetdevUserOptions;

typedef struct NetdevUserOptionsList
{
    union {
        NetdevUserOptions *value;
        uint64_t padding;
    };
    struct NetdevUserOptionsList *next;
} NetdevUserOptionsList;


typedef struct NetdevTapOptions NetdevTapOptions;

typedef struct NetdevTapOptionsList
{
    union {
        NetdevTapOptions *value;
        uint64_t padding;
    };
    struct NetdevTapOptionsList *next;
} NetdevTapOptionsList;


typedef struct NetdevSocketOptions NetdevSocketOptions;

typedef struct NetdevSocketOptionsList
{
    union {
        NetdevSocketOptions *value;
        uint64_t padding;
    };
    struct NetdevSocketOptionsList *next;
} NetdevSocketOptionsList;


typedef struct NetdevL2TPv3Options NetdevL2TPv3Options;

typedef struct NetdevL2TPv3OptionsList
{
    union {
        NetdevL2TPv3Options *value;
        uint64_t padding;
    };
    struct NetdevL2TPv3OptionsList *next;
} NetdevL2TPv3OptionsList;


typedef struct NetdevVdeOptions NetdevVdeOptions;

typedef struct NetdevVdeOptionsList
{
    union {
        NetdevVdeOptions *value;
        uint64_t padding;
    };
    struct NetdevVdeOptionsList *next;
} NetdevVdeOptionsList;


typedef struct NetdevDumpOptions NetdevDumpOptions;

typedef struct NetdevDumpOptionsList
{
    union {
        NetdevDumpOptions *value;
        uint64_t padding;
    };
    struct NetdevDumpOptionsList *next;
} NetdevDumpOptionsList;


typedef struct NetdevBridgeOptions NetdevBridgeOptions;

typedef struct NetdevBridgeOptionsList
{
    union {
        NetdevBridgeOptions *value;
        uint64_t padding;
    };
    struct NetdevBridgeOptionsList *next;
} NetdevBridgeOptionsList;


typedef struct NetdevHubPortOptions NetdevHubPortOptions;

typedef struct NetdevHubPortOptionsList
{
    union {
        NetdevHubPortOptions *value;
        uint64_t padding;
    };
    struct NetdevHubPortOptionsList *next;
} NetdevHubPortOptionsList;


typedef struct NetdevNetmapOptions NetdevNetmapOptions;

typedef struct NetdevNetmapOptionsList
{
    union {
        NetdevNetmapOptions *value;
        uint64_t padding;
    };
    struct NetdevNetmapOptionsList *next;
} NetdevNetmapOptionsList;


typedef struct NetdevVhostUserOptions NetdevVhostUserOptions;

typedef struct NetdevVhostUserOptionsList
{
    union {
        NetdevVhostUserOptions *value;
        uint64_t padding;
    };
    struct NetdevVhostUserOptionsList *next;
} NetdevVhostUserOptionsList;


typedef struct NetdevVnetOptions NetdevVnetOptions;

typedef struct NetdevVnetOptionsList
{
    union {
        NetdevVnetOptions *value;
        uint64_t padding;
    };
    struct NetdevVnetOptionsList *next;
} NetdevVnetOptionsList;

typedef struct NetClientOptions NetClientOptions;

typedef struct NetClientOptionsList
{
    union {
        NetClientOptions *value;
        uint64_t padding;
    };
    struct NetClientOptionsList *next;
} NetClientOptionsList;

extern const char *NetClientOptionsKind_lookup[];
typedef enum NetClientOptionsKind
{
    NET_CLIENT_OPTIONS_KIND_NONE = 0,
    NET_CLIENT_OPTIONS_KIND_NIC = 1,
    NET_CLIENT_OPTIONS_KIND_USER = 2,
    NET_CLIENT_OPTIONS_KIND_TAP = 3,
    NET_CLIENT_OPTIONS_KIND_L2TPV3 = 4,
    NET_CLIENT_OPTIONS_KIND_SOCKET = 5,
    NET_CLIENT_OPTIONS_KIND_VDE = 6,
    NET_CLIENT_OPTIONS_KIND_DUMP = 7,
    NET_CLIENT_OPTIONS_KIND_BRIDGE = 8,
    NET_CLIENT_OPTIONS_KIND_HUBPORT = 9,
    NET_CLIENT_OPTIONS_KIND_NETMAP = 10,
    NET_CLIENT_OPTIONS_KIND_VHOST_USER = 11,
    NET_CLIENT_OPTIONS_KIND_VNET = 12,
    NET_CLIENT_OPTIONS_KIND_MAX = 13,
} NetClientOptionsKind;


typedef struct NetLegacy NetLegacy;

typedef struct NetLegacyList
{
    union {
        NetLegacy *value;
        uint64_t padding;
    };
    struct NetLegacyList *next;
} NetLegacyList;


typedef struct Netdev Netdev;

typedef struct NetdevList
{
    union {
        Netdev *value;
        uint64_t padding;
    };
    struct NetdevList *next;
} NetdevList;


typedef struct InetSocketAddress InetSocketAddress;

typedef struct InetSocketAddressList
{
    union {
        InetSocketAddress *value;
        uint64_t padding;
    };
    struct InetSocketAddressList *next;
} InetSocketAddressList;


typedef struct UnixSocketAddress UnixSocketAddress;

typedef struct UnixSocketAddressList
{
    union {
        UnixSocketAddress *value;
        uint64_t padding;
    };
    struct UnixSocketAddressList *next;
} UnixSocketAddressList;


typedef struct SocketAddress SocketAddress;

typedef struct SocketAddressList
{
    union {
        SocketAddress *value;
        uint64_t padding;
    };
    struct SocketAddressList *next;
} SocketAddressList;

extern const char *SocketAddressKind_lookup[];
typedef enum SocketAddressKind
{
    SOCKET_ADDRESS_KIND_INET = 0,
    SOCKET_ADDRESS_KIND_UNIX = 1,
    SOCKET_ADDRESS_KIND_FD = 2,
    SOCKET_ADDRESS_KIND_MAX = 3,
} SocketAddressKind;


typedef struct MachineInfo MachineInfo;

typedef struct MachineInfoList
{
    union {
        MachineInfo *value;
        uint64_t padding;
    };
    struct MachineInfoList *next;
} MachineInfoList;


typedef struct CpuDefinitionInfo CpuDefinitionInfo;

typedef struct CpuDefinitionInfoList
{
    union {
        CpuDefinitionInfo *value;
        uint64_t padding;
    };
    struct CpuDefinitionInfoList *next;
} CpuDefinitionInfoList;


typedef struct AddfdInfo AddfdInfo;

typedef struct AddfdInfoList
{
    union {
        AddfdInfo *value;
        uint64_t padding;
    };
    struct AddfdInfoList *next;
} AddfdInfoList;


typedef struct FdsetFdInfo FdsetFdInfo;

typedef struct FdsetFdInfoList
{
    union {
        FdsetFdInfo *value;
        uint64_t padding;
    };
    struct FdsetFdInfoList *next;
} FdsetFdInfoList;


typedef struct FdsetInfo FdsetInfo;

typedef struct FdsetInfoList
{
    union {
        FdsetInfo *value;
        uint64_t padding;
    };
    struct FdsetInfoList *next;
} FdsetInfoList;


typedef struct TargetInfo TargetInfo;

typedef struct TargetInfoList
{
    union {
        TargetInfo *value;
        uint64_t padding;
    };
    struct TargetInfoList *next;
} TargetInfoList;

extern const char *QKeyCode_lookup[];
typedef enum QKeyCode
{
    Q_KEY_CODE_UNMAPPED = 0,
    Q_KEY_CODE_SHIFT = 1,
    Q_KEY_CODE_SHIFT_R = 2,
    Q_KEY_CODE_ALT = 3,
    Q_KEY_CODE_ALT_R = 4,
    Q_KEY_CODE_ALTGR = 5,
    Q_KEY_CODE_ALTGR_R = 6,
    Q_KEY_CODE_CTRL = 7,
    Q_KEY_CODE_CTRL_R = 8,
    Q_KEY_CODE_MENU = 9,
    Q_KEY_CODE_ESC = 10,
    Q_KEY_CODE_1 = 11,
    Q_KEY_CODE_2 = 12,
    Q_KEY_CODE_3 = 13,
    Q_KEY_CODE_4 = 14,
    Q_KEY_CODE_5 = 15,
    Q_KEY_CODE_6 = 16,
    Q_KEY_CODE_7 = 17,
    Q_KEY_CODE_8 = 18,
    Q_KEY_CODE_9 = 19,
    Q_KEY_CODE_0 = 20,
    Q_KEY_CODE_MINUS = 21,
    Q_KEY_CODE_EQUAL = 22,
    Q_KEY_CODE_BACKSPACE = 23,
    Q_KEY_CODE_TAB = 24,
    Q_KEY_CODE_Q = 25,
    Q_KEY_CODE_W = 26,
    Q_KEY_CODE_E = 27,
    Q_KEY_CODE_R = 28,
    Q_KEY_CODE_T = 29,
    Q_KEY_CODE_Y = 30,
    Q_KEY_CODE_U = 31,
    Q_KEY_CODE_I = 32,
    Q_KEY_CODE_O = 33,
    Q_KEY_CODE_P = 34,
    Q_KEY_CODE_BRACKET_LEFT = 35,
    Q_KEY_CODE_BRACKET_RIGHT = 36,
    Q_KEY_CODE_RET = 37,
    Q_KEY_CODE_A = 38,
    Q_KEY_CODE_S = 39,
    Q_KEY_CODE_D = 40,
    Q_KEY_CODE_F = 41,
    Q_KEY_CODE_G = 42,
    Q_KEY_CODE_H = 43,
    Q_KEY_CODE_J = 44,
    Q_KEY_CODE_K = 45,
    Q_KEY_CODE_L = 46,
    Q_KEY_CODE_SEMICOLON = 47,
    Q_KEY_CODE_APOSTROPHE = 48,
    Q_KEY_CODE_GRAVE_ACCENT = 49,
    Q_KEY_CODE_BACKSLASH = 50,
    Q_KEY_CODE_Z = 51,
    Q_KEY_CODE_X = 52,
    Q_KEY_CODE_C = 53,
    Q_KEY_CODE_V = 54,
    Q_KEY_CODE_B = 55,
    Q_KEY_CODE_N = 56,
    Q_KEY_CODE_M = 57,
    Q_KEY_CODE_COMMA = 58,
    Q_KEY_CODE_DOT = 59,
    Q_KEY_CODE_SLASH = 60,
    Q_KEY_CODE_ASTERISK = 61,
    Q_KEY_CODE_SPC = 62,
    Q_KEY_CODE_CAPS_LOCK = 63,
    Q_KEY_CODE_F1 = 64,
    Q_KEY_CODE_F2 = 65,
    Q_KEY_CODE_F3 = 66,
    Q_KEY_CODE_F4 = 67,
    Q_KEY_CODE_F5 = 68,
    Q_KEY_CODE_F6 = 69,
    Q_KEY_CODE_F7 = 70,
    Q_KEY_CODE_F8 = 71,
    Q_KEY_CODE_F9 = 72,
    Q_KEY_CODE_F10 = 73,
    Q_KEY_CODE_NUM_LOCK = 74,
    Q_KEY_CODE_SCROLL_LOCK = 75,
    Q_KEY_CODE_KP_DIVIDE = 76,
    Q_KEY_CODE_KP_MULTIPLY = 77,
    Q_KEY_CODE_KP_SUBTRACT = 78,
    Q_KEY_CODE_KP_ADD = 79,
    Q_KEY_CODE_KP_ENTER = 80,
    Q_KEY_CODE_KP_DECIMAL = 81,
    Q_KEY_CODE_SYSRQ = 82,
    Q_KEY_CODE_KP_0 = 83,
    Q_KEY_CODE_KP_1 = 84,
    Q_KEY_CODE_KP_2 = 85,
    Q_KEY_CODE_KP_3 = 86,
    Q_KEY_CODE_KP_4 = 87,
    Q_KEY_CODE_KP_5 = 88,
    Q_KEY_CODE_KP_6 = 89,
    Q_KEY_CODE_KP_7 = 90,
    Q_KEY_CODE_KP_8 = 91,
    Q_KEY_CODE_KP_9 = 92,
    Q_KEY_CODE_LESS = 93,
    Q_KEY_CODE_F11 = 94,
    Q_KEY_CODE_F12 = 95,
    Q_KEY_CODE_PRINT = 96,
    Q_KEY_CODE_HOME = 97,
    Q_KEY_CODE_PGUP = 98,
    Q_KEY_CODE_PGDN = 99,
    Q_KEY_CODE_END = 100,
    Q_KEY_CODE_LEFT = 101,
    Q_KEY_CODE_UP = 102,
    Q_KEY_CODE_DOWN = 103,
    Q_KEY_CODE_RIGHT = 104,
    Q_KEY_CODE_INSERT = 105,
    Q_KEY_CODE_DELETE = 106,
    Q_KEY_CODE_STOP = 107,
    Q_KEY_CODE_AGAIN = 108,
    Q_KEY_CODE_PROPS = 109,
    Q_KEY_CODE_UNDO = 110,
    Q_KEY_CODE_FRONT = 111,
    Q_KEY_CODE_COPY = 112,
    Q_KEY_CODE_OPEN = 113,
    Q_KEY_CODE_PASTE = 114,
    Q_KEY_CODE_FIND = 115,
    Q_KEY_CODE_CUT = 116,
    Q_KEY_CODE_LF = 117,
    Q_KEY_CODE_HELP = 118,
    Q_KEY_CODE_META_L = 119,
    Q_KEY_CODE_META_R = 120,
    Q_KEY_CODE_COMPOSE = 121,
    Q_KEY_CODE_PAUSE = 122,
    Q_KEY_CODE_MAX = 123,
} QKeyCode;

typedef struct QKeyCodeList
{
    union {
        QKeyCode value;
        uint64_t padding;
    };
    struct QKeyCodeList *next;
} QKeyCodeList;


typedef struct KeyValue KeyValue;

typedef struct KeyValueList
{
    union {
        KeyValue *value;
        uint64_t padding;
    };
    struct KeyValueList *next;
} KeyValueList;

extern const char *KeyValueKind_lookup[];
typedef enum KeyValueKind
{
    KEY_VALUE_KIND_NUMBER = 0,
    KEY_VALUE_KIND_QCODE = 1,
    KEY_VALUE_KIND_MAX = 2,
} KeyValueKind;


typedef struct ChardevFile ChardevFile;

typedef struct ChardevFileList
{
    union {
        ChardevFile *value;
        uint64_t padding;
    };
    struct ChardevFileList *next;
} ChardevFileList;


typedef struct ChardevHostdev ChardevHostdev;

typedef struct ChardevHostdevList
{
    union {
        ChardevHostdev *value;
        uint64_t padding;
    };
    struct ChardevHostdevList *next;
} ChardevHostdevList;


typedef struct ChardevSocket ChardevSocket;

typedef struct ChardevSocketList
{
    union {
        ChardevSocket *value;
        uint64_t padding;
    };
    struct ChardevSocketList *next;
} ChardevSocketList;


typedef struct ChardevUdp ChardevUdp;

typedef struct ChardevUdpList
{
    union {
        ChardevUdp *value;
        uint64_t padding;
    };
    struct ChardevUdpList *next;
} ChardevUdpList;


typedef struct ChardevMux ChardevMux;

typedef struct ChardevMuxList
{
    union {
        ChardevMux *value;
        uint64_t padding;
    };
    struct ChardevMuxList *next;
} ChardevMuxList;


typedef struct ChardevStdio ChardevStdio;

typedef struct ChardevStdioList
{
    union {
        ChardevStdio *value;
        uint64_t padding;
    };
    struct ChardevStdioList *next;
} ChardevStdioList;


typedef struct ChardevSpiceChannel ChardevSpiceChannel;

typedef struct ChardevSpiceChannelList
{
    union {
        ChardevSpiceChannel *value;
        uint64_t padding;
    };
    struct ChardevSpiceChannelList *next;
} ChardevSpiceChannelList;


typedef struct ChardevSpicePort ChardevSpicePort;

typedef struct ChardevSpicePortList
{
    union {
        ChardevSpicePort *value;
        uint64_t padding;
    };
    struct ChardevSpicePortList *next;
} ChardevSpicePortList;


typedef struct ChardevVC ChardevVC;

typedef struct ChardevVCList
{
    union {
        ChardevVC *value;
        uint64_t padding;
    };
    struct ChardevVCList *next;
} ChardevVCList;


typedef struct ChardevRingbuf ChardevRingbuf;

typedef struct ChardevRingbufList
{
    union {
        ChardevRingbuf *value;
        uint64_t padding;
    };
    struct ChardevRingbufList *next;
} ChardevRingbufList;


typedef struct ChardevDummy ChardevDummy;

typedef struct ChardevDummyList
{
    union {
        ChardevDummy *value;
        uint64_t padding;
    };
    struct ChardevDummyList *next;
} ChardevDummyList;


typedef struct ChardevBackend ChardevBackend;

typedef struct ChardevBackendList
{
    union {
        ChardevBackend *value;
        uint64_t padding;
    };
    struct ChardevBackendList *next;
} ChardevBackendList;

extern const char *ChardevBackendKind_lookup[];
typedef enum ChardevBackendKind
{
    CHARDEV_BACKEND_KIND_FILE = 0,
    CHARDEV_BACKEND_KIND_SERIAL = 1,
    CHARDEV_BACKEND_KIND_PARALLEL = 2,
    CHARDEV_BACKEND_KIND_PIPE = 3,
    CHARDEV_BACKEND_KIND_SOCKET = 4,
    CHARDEV_BACKEND_KIND_UDP = 5,
    CHARDEV_BACKEND_KIND_PTY = 6,
    CHARDEV_BACKEND_KIND_NULL = 7,
    CHARDEV_BACKEND_KIND_MUX = 8,
    CHARDEV_BACKEND_KIND_MSMOUSE = 9,
    CHARDEV_BACKEND_KIND_BRAILLE = 10,
    CHARDEV_BACKEND_KIND_TESTDEV = 11,
    CHARDEV_BACKEND_KIND_STDIO = 12,
    CHARDEV_BACKEND_KIND_CONSOLE = 13,
    CHARDEV_BACKEND_KIND_SPICEVMC = 14,
    CHARDEV_BACKEND_KIND_SPICEPORT = 15,
    CHARDEV_BACKEND_KIND_VC = 16,
    CHARDEV_BACKEND_KIND_RINGBUF = 17,
    CHARDEV_BACKEND_KIND_MEMORY = 18,
    CHARDEV_BACKEND_KIND_MAX = 19,
} ChardevBackendKind;


typedef struct ChardevReturn ChardevReturn;

typedef struct ChardevReturnList
{
    union {
        ChardevReturn *value;
        uint64_t padding;
    };
    struct ChardevReturnList *next;
} ChardevReturnList;

extern const char *TpmModel_lookup[];
typedef enum TpmModel
{
    TPM_MODEL_TPM_TIS = 0,
    TPM_MODEL_MAX = 1,
} TpmModel;

typedef struct TpmModelList
{
    union {
        TpmModel value;
        uint64_t padding;
    };
    struct TpmModelList *next;
} TpmModelList;

extern const char *TpmType_lookup[];
typedef enum TpmType
{
    TPM_TYPE_PASSTHROUGH = 0,
    TPM_TYPE_MAX = 1,
} TpmType;

typedef struct TpmTypeList
{
    union {
        TpmType value;
        uint64_t padding;
    };
    struct TpmTypeList *next;
} TpmTypeList;


typedef struct TPMPassthroughOptions TPMPassthroughOptions;

typedef struct TPMPassthroughOptionsList
{
    union {
        TPMPassthroughOptions *value;
        uint64_t padding;
    };
    struct TPMPassthroughOptionsList *next;
} TPMPassthroughOptionsList;


typedef struct TpmTypeOptions TpmTypeOptions;

typedef struct TpmTypeOptionsList
{
    union {
        TpmTypeOptions *value;
        uint64_t padding;
    };
    struct TpmTypeOptionsList *next;
} TpmTypeOptionsList;

extern const char *TpmTypeOptionsKind_lookup[];
typedef enum TpmTypeOptionsKind
{
    TPM_TYPE_OPTIONS_KIND_PASSTHROUGH = 0,
    TPM_TYPE_OPTIONS_KIND_MAX = 1,
} TpmTypeOptionsKind;


typedef struct TPMInfo TPMInfo;

typedef struct TPMInfoList
{
    union {
        TPMInfo *value;
        uint64_t padding;
    };
    struct TPMInfoList *next;
} TPMInfoList;


typedef struct AcpiTableOptions AcpiTableOptions;

typedef struct AcpiTableOptionsList
{
    union {
        AcpiTableOptions *value;
        uint64_t padding;
    };
    struct AcpiTableOptionsList *next;
} AcpiTableOptionsList;

extern const char *CommandLineParameterType_lookup[];
typedef enum CommandLineParameterType
{
    COMMAND_LINE_PARAMETER_TYPE_STRING = 0,
    COMMAND_LINE_PARAMETER_TYPE_BOOLEAN = 1,
    COMMAND_LINE_PARAMETER_TYPE_NUMBER = 2,
    COMMAND_LINE_PARAMETER_TYPE_SIZE = 3,
    COMMAND_LINE_PARAMETER_TYPE_MAX = 4,
} CommandLineParameterType;

typedef struct CommandLineParameterTypeList
{
    union {
        CommandLineParameterType value;
        uint64_t padding;
    };
    struct CommandLineParameterTypeList *next;
} CommandLineParameterTypeList;


typedef struct CommandLineParameterInfo CommandLineParameterInfo;

typedef struct CommandLineParameterInfoList
{
    union {
        CommandLineParameterInfo *value;
        uint64_t padding;
    };
    struct CommandLineParameterInfoList *next;
} CommandLineParameterInfoList;


typedef struct CommandLineOptionInfo CommandLineOptionInfo;

typedef struct CommandLineOptionInfoList
{
    union {
        CommandLineOptionInfo *value;
        uint64_t padding;
    };
    struct CommandLineOptionInfoList *next;
} CommandLineOptionInfoList;

extern const char *X86CPURegister32_lookup[];
typedef enum X86CPURegister32
{
    X86_CPU_REGISTER32_EAX = 0,
    X86_CPU_REGISTER32_EBX = 1,
    X86_CPU_REGISTER32_ECX = 2,
    X86_CPU_REGISTER32_EDX = 3,
    X86_CPU_REGISTER32_ESP = 4,
    X86_CPU_REGISTER32_EBP = 5,
    X86_CPU_REGISTER32_ESI = 6,
    X86_CPU_REGISTER32_EDI = 7,
    X86_CPU_REGISTER32_MAX = 8,
} X86CPURegister32;

typedef struct X86CPURegister32List
{
    union {
        X86CPURegister32 value;
        uint64_t padding;
    };
    struct X86CPURegister32List *next;
} X86CPURegister32List;


typedef struct X86CPUFeatureWordInfo X86CPUFeatureWordInfo;

typedef struct X86CPUFeatureWordInfoList
{
    union {
        X86CPUFeatureWordInfo *value;
        uint64_t padding;
    };
    struct X86CPUFeatureWordInfoList *next;
} X86CPUFeatureWordInfoList;

extern const char *RxState_lookup[];
typedef enum RxState
{
    RX_STATE_NORMAL = 0,
    RX_STATE_NONE = 1,
    RX_STATE_ALL = 2,
    RX_STATE_MAX = 3,
} RxState;

typedef struct RxStateList
{
    union {
        RxState value;
        uint64_t padding;
    };
    struct RxStateList *next;
} RxStateList;


typedef struct RxFilterInfo RxFilterInfo;

typedef struct RxFilterInfoList
{
    union {
        RxFilterInfo *value;
        uint64_t padding;
    };
    struct RxFilterInfoList *next;
} RxFilterInfoList;

extern const char *InputButton_lookup[];
typedef enum InputButton
{
    INPUT_BUTTON_LEFT = 0,
    INPUT_BUTTON_MIDDLE = 1,
    INPUT_BUTTON_RIGHT = 2,
    INPUT_BUTTON_WHEEL_UP = 3,
    INPUT_BUTTON_WHEEL_DOWN = 4,
    INPUT_BUTTON_MAX = 5,
} InputButton;

typedef struct InputButtonList
{
    union {
        InputButton value;
        uint64_t padding;
    };
    struct InputButtonList *next;
} InputButtonList;

extern const char *InputAxis_lookup[];
typedef enum InputAxis
{
    INPUT_AXIS_X = 0,
    INPUT_AXIS_Y = 1,
    INPUT_AXIS_Z = 2,
    INPUT_AXIS_MAX = 3,
} InputAxis;

typedef struct InputAxisList
{
    union {
        InputAxis value;
        uint64_t padding;
    };
    struct InputAxisList *next;
} InputAxisList;


typedef struct InputKeyEvent InputKeyEvent;

typedef struct InputKeyEventList
{
    union {
        InputKeyEvent *value;
        uint64_t padding;
    };
    struct InputKeyEventList *next;
} InputKeyEventList;


typedef struct InputBtnEvent InputBtnEvent;

typedef struct InputBtnEventList
{
    union {
        InputBtnEvent *value;
        uint64_t padding;
    };
    struct InputBtnEventList *next;
} InputBtnEventList;


typedef struct InputMoveEvent InputMoveEvent;

typedef struct InputMoveEventList
{
    union {
        InputMoveEvent *value;
        uint64_t padding;
    };
    struct InputMoveEventList *next;
} InputMoveEventList;


typedef struct InputEvent InputEvent;

typedef struct InputEventList
{
    union {
        InputEvent *value;
        uint64_t padding;
    };
    struct InputEventList *next;
} InputEventList;

extern const char *InputEventKind_lookup[];
typedef enum InputEventKind
{
    INPUT_EVENT_KIND_KEY = 0,
    INPUT_EVENT_KIND_BTN = 1,
    INPUT_EVENT_KIND_REL = 2,
    INPUT_EVENT_KIND_ABS = 3,
    INPUT_EVENT_KIND_MAX = 4,
} InputEventKind;


typedef struct NumaOptions NumaOptions;

typedef struct NumaOptionsList
{
    union {
        NumaOptions *value;
        uint64_t padding;
    };
    struct NumaOptionsList *next;
} NumaOptionsList;

extern const char *NumaOptionsKind_lookup[];
typedef enum NumaOptionsKind
{
    NUMA_OPTIONS_KIND_NODE = 0,
    NUMA_OPTIONS_KIND_MAX = 1,
} NumaOptionsKind;


typedef struct NumaNodeOptions NumaNodeOptions;

typedef struct NumaNodeOptionsList
{
    union {
        NumaNodeOptions *value;
        uint64_t padding;
    };
    struct NumaNodeOptionsList *next;
} NumaNodeOptionsList;

extern const char *HostMemPolicy_lookup[];
typedef enum HostMemPolicy
{
    HOST_MEM_POLICY_DEFAULT = 0,
    HOST_MEM_POLICY_PREFERRED = 1,
    HOST_MEM_POLICY_BIND = 2,
    HOST_MEM_POLICY_INTERLEAVE = 3,
    HOST_MEM_POLICY_MAX = 4,
} HostMemPolicy;

typedef struct HostMemPolicyList
{
    union {
        HostMemPolicy value;
        uint64_t padding;
    };
    struct HostMemPolicyList *next;
} HostMemPolicyList;


typedef struct Memdev Memdev;

typedef struct MemdevList
{
    union {
        Memdev *value;
        uint64_t padding;
    };
    struct MemdevList *next;
} MemdevList;


typedef struct PCDIMMDeviceInfo PCDIMMDeviceInfo;

typedef struct PCDIMMDeviceInfoList
{
    union {
        PCDIMMDeviceInfo *value;
        uint64_t padding;
    };
    struct PCDIMMDeviceInfoList *next;
} PCDIMMDeviceInfoList;


typedef struct MemoryDeviceInfo MemoryDeviceInfo;

typedef struct MemoryDeviceInfoList
{
    union {
        MemoryDeviceInfo *value;
        uint64_t padding;
    };
    struct MemoryDeviceInfoList *next;
} MemoryDeviceInfoList;

extern const char *MemoryDeviceInfoKind_lookup[];
typedef enum MemoryDeviceInfoKind
{
    MEMORY_DEVICE_INFO_KIND_DIMM = 0,
    MEMORY_DEVICE_INFO_KIND_MAX = 1,
} MemoryDeviceInfoKind;

extern const char *ACPISlotType_lookup[];
typedef enum ACPISlotType
{
    ACPI_SLOT_TYPE_DIMM = 0,
    ACPI_SLOT_TYPE_MAX = 1,
} ACPISlotType;

typedef struct ACPISlotTypeList
{
    union {
        ACPISlotType value;
        uint64_t padding;
    };
    struct ACPISlotTypeList *next;
} ACPISlotTypeList;


typedef struct ACPIOSTInfo ACPIOSTInfo;

typedef struct ACPIOSTInfoList
{
    union {
        ACPIOSTInfo *value;
        uint64_t padding;
    };
    struct ACPIOSTInfoList *next;
} ACPIOSTInfoList;

extern const char *WatchdogExpirationAction_lookup[];
typedef enum WatchdogExpirationAction
{
    WATCHDOG_EXPIRATION_ACTION_RESET = 0,
    WATCHDOG_EXPIRATION_ACTION_SHUTDOWN = 1,
    WATCHDOG_EXPIRATION_ACTION_POWEROFF = 2,
    WATCHDOG_EXPIRATION_ACTION_PAUSE = 3,
    WATCHDOG_EXPIRATION_ACTION_DEBUG = 4,
    WATCHDOG_EXPIRATION_ACTION_NONE = 5,
    WATCHDOG_EXPIRATION_ACTION_MAX = 6,
} WatchdogExpirationAction;

typedef struct WatchdogExpirationActionList
{
    union {
        WatchdogExpirationAction value;
        uint64_t padding;
    };
    struct WatchdogExpirationActionList *next;
} WatchdogExpirationActionList;

extern const char *IoOperationType_lookup[];
typedef enum IoOperationType
{
    IO_OPERATION_TYPE_READ = 0,
    IO_OPERATION_TYPE_WRITE = 1,
    IO_OPERATION_TYPE_MAX = 2,
} IoOperationType;

typedef struct IoOperationTypeList
{
    union {
        IoOperationType value;
        uint64_t padding;
    };
    struct IoOperationTypeList *next;
} IoOperationTypeList;

extern const char *GuestPanicAction_lookup[];
typedef enum GuestPanicAction
{
    GUEST_PANIC_ACTION_PAUSE = 0,
    GUEST_PANIC_ACTION_MAX = 1,
} GuestPanicAction;

typedef struct GuestPanicActionList
{
    union {
        GuestPanicAction value;
        uint64_t padding;
    };
    struct GuestPanicActionList *next;
} GuestPanicActionList;

#ifndef QAPI_TYPES_BUILTIN_CLEANUP_DECL_H
#define QAPI_TYPES_BUILTIN_CLEANUP_DECL_H

void qapi_free_strList(strList *obj);
void qapi_free_intList(intList *obj);
void qapi_free_numberList(numberList *obj);
void qapi_free_boolList(boolList *obj);
void qapi_free_int8List(int8List *obj);
void qapi_free_int16List(int16List *obj);
void qapi_free_int32List(int32List *obj);
void qapi_free_int64List(int64List *obj);
void qapi_free_uint8List(uint8List *obj);
void qapi_free_uint16List(uint16List *obj);
void qapi_free_uint32List(uint32List *obj);
void qapi_free_uint64List(uint64List *obj);

#endif /* QAPI_TYPES_BUILTIN_CLEANUP_DECL_H */


void qapi_free_ErrorClassList(ErrorClassList *obj);

struct VersionInfo
{
    struct 
    {
        int64_t major;
        int64_t minor;
        int64_t micro;
    } qemu;
    char *package;
};

void qapi_free_VersionInfoList(VersionInfoList *obj);
void qapi_free_VersionInfo(VersionInfo *obj);

struct CommandInfo
{
    char *name;
};

void qapi_free_CommandInfoList(CommandInfoList *obj);
void qapi_free_CommandInfo(CommandInfo *obj);

void qapi_free_OnOffAutoList(OnOffAutoList *obj);

struct SnapshotInfo
{
    char *id;
    char *name;
    int64_t vm_state_size;
    int64_t date_sec;
    int64_t date_nsec;
    int64_t vm_clock_sec;
    int64_t vm_clock_nsec;
};

void qapi_free_SnapshotInfoList(SnapshotInfoList *obj);
void qapi_free_SnapshotInfo(SnapshotInfo *obj);

struct ImageInfoSpecificQCow2
{
    char *compat;
    bool has_lazy_refcounts;
    bool lazy_refcounts;
    bool has_corrupt;
    bool corrupt;
};

void qapi_free_ImageInfoSpecificQCow2List(ImageInfoSpecificQCow2List *obj);
void qapi_free_ImageInfoSpecificQCow2(ImageInfoSpecificQCow2 *obj);

struct ImageInfoSpecificVmdk
{
    char *create_type;
    int64_t cid;
    int64_t parent_cid;
    ImageInfoList *extents;
};

void qapi_free_ImageInfoSpecificVmdkList(ImageInfoSpecificVmdkList *obj);
void qapi_free_ImageInfoSpecificVmdk(ImageInfoSpecificVmdk *obj);

struct ImageInfoSpecific
{
    ImageInfoSpecificKind kind;
    union {
        void *data;
        ImageInfoSpecificQCow2 *qcow2;
        ImageInfoSpecificVmdk *vmdk;
    };
};
void qapi_free_ImageInfoSpecificList(ImageInfoSpecificList *obj);
void qapi_free_ImageInfoSpecific(ImageInfoSpecific *obj);

struct ImageInfo
{
    char *filename;
    char *format;
    bool has_dirty_flag;
    bool dirty_flag;
    bool has_actual_size;
    int64_t actual_size;
    int64_t virtual_size;
    bool has_cluster_size;
    int64_t cluster_size;
    bool has_encrypted;
    bool encrypted;
    bool has_compressed;
    bool compressed;
    bool has_backing_filename;
    char *backing_filename;
    bool has_full_backing_filename;
    char *full_backing_filename;
    bool has_backing_filename_format;
    char *backing_filename_format;
    bool has_snapshots;
    SnapshotInfoList *snapshots;
    bool has_backing_image;
    ImageInfo *backing_image;
    bool has_format_specific;
    ImageInfoSpecific *format_specific;
};

void qapi_free_ImageInfoList(ImageInfoList *obj);
void qapi_free_ImageInfo(ImageInfo *obj);

struct ImageCheck
{
    char *filename;
    char *format;
    int64_t check_errors;
    bool has_image_end_offset;
    int64_t image_end_offset;
    bool has_corruptions;
    int64_t corruptions;
    bool has_leaks;
    int64_t leaks;
    bool has_corruptions_fixed;
    int64_t corruptions_fixed;
    bool has_leaks_fixed;
    int64_t leaks_fixed;
    bool has_total_clusters;
    int64_t total_clusters;
    bool has_allocated_clusters;
    int64_t allocated_clusters;
    bool has_fragmented_clusters;
    int64_t fragmented_clusters;
    bool has_compressed_clusters;
    int64_t compressed_clusters;
};

void qapi_free_ImageCheckList(ImageCheckList *obj);
void qapi_free_ImageCheck(ImageCheck *obj);

#undef direct

struct BlockdevCacheInfo
{
    bool writeback;
    bool direct;
    bool no_flush;
};

void qapi_free_BlockdevCacheInfoList(BlockdevCacheInfoList *obj);
void qapi_free_BlockdevCacheInfo(BlockdevCacheInfo *obj);

struct BlockDeviceInfo
{
    char *file;
    bool has_node_name;
    char *node_name;
    bool ro;
    char *drv;
    bool has_backing_file;
    char *backing_file;
    int64_t backing_file_depth;
    bool encrypted;
    bool encryption_key_missing;
    BlockdevDetectZeroesOptions detect_zeroes;
    int64_t bps;
    int64_t bps_rd;
    int64_t bps_wr;
    int64_t iops;
    int64_t iops_rd;
    int64_t iops_wr;
    ImageInfo *image;
    bool has_bps_max;
    int64_t bps_max;
    bool has_bps_rd_max;
    int64_t bps_rd_max;
    bool has_bps_wr_max;
    int64_t bps_wr_max;
    bool has_iops_max;
    int64_t iops_max;
    bool has_iops_rd_max;
    int64_t iops_rd_max;
    bool has_iops_wr_max;
    int64_t iops_wr_max;
    bool has_iops_size;
    int64_t iops_size;
    BlockdevCacheInfo *cache;
};

void qapi_free_BlockDeviceInfoList(BlockDeviceInfoList *obj);
void qapi_free_BlockDeviceInfo(BlockDeviceInfo *obj);

void qapi_free_BlockDeviceIoStatusList(BlockDeviceIoStatusList *obj);

struct BlockDeviceMapEntry
{
    int64_t start;
    int64_t length;
    int64_t depth;
    bool zero;
    bool data;
    bool has_offset;
    int64_t offset;
};

void qapi_free_BlockDeviceMapEntryList(BlockDeviceMapEntryList *obj);
void qapi_free_BlockDeviceMapEntry(BlockDeviceMapEntry *obj);

struct BlockDirtyInfo
{
    int64_t count;
    int64_t granularity;
};

void qapi_free_BlockDirtyInfoList(BlockDirtyInfoList *obj);
void qapi_free_BlockDirtyInfo(BlockDirtyInfo *obj);

struct BlockInfo
{
    char *device;
    char *type;
    bool removable;
    bool locked;
    bool has_inserted;
    BlockDeviceInfo *inserted;
    bool has_tray_open;
    bool tray_open;
    bool has_io_status;
    BlockDeviceIoStatus io_status;
    bool has_dirty_bitmaps;
    BlockDirtyInfoList *dirty_bitmaps;
};

void qapi_free_BlockInfoList(BlockInfoList *obj);
void qapi_free_BlockInfo(BlockInfo *obj);

struct BlockDeviceStats
{
    int64_t rd_bytes;
    int64_t wr_bytes;
    int64_t rd_operations;
    int64_t wr_operations;
    int64_t flush_operations;
    int64_t flush_total_time_ns;
    int64_t wr_total_time_ns;
    int64_t rd_total_time_ns;
    int64_t wr_highest_offset;
};

void qapi_free_BlockDeviceStatsList(BlockDeviceStatsList *obj);
void qapi_free_BlockDeviceStats(BlockDeviceStats *obj);

struct BlockStats
{
    bool has_device;
    char *device;
    bool has_node_name;
    char *node_name;
    BlockDeviceStats *stats;
    bool has_parent;
    BlockStats *parent;
    bool has_backing;
    BlockStats *backing;
};

void qapi_free_BlockStatsList(BlockStatsList *obj);
void qapi_free_BlockStats(BlockStats *obj);

void qapi_free_BlockdevOnErrorList(BlockdevOnErrorList *obj);

void qapi_free_MirrorSyncModeList(MirrorSyncModeList *obj);

void qapi_free_BlockJobTypeList(BlockJobTypeList *obj);

struct BlockJobInfo
{
    char *type;
    char *device;
    int64_t len;
    int64_t offset;
    bool busy;
    bool paused;
    int64_t speed;
    BlockDeviceIoStatus io_status;
    bool ready;
};

void qapi_free_BlockJobInfoList(BlockJobInfoList *obj);
void qapi_free_BlockJobInfo(BlockJobInfo *obj);

void qapi_free_NewImageModeList(NewImageModeList *obj);

struct BlockdevSnapshot
{
    bool has_device;
    char *device;
    bool has_node_name;
    char *node_name;
    char *snapshot_file;
    bool has_snapshot_node_name;
    char *snapshot_node_name;
    bool has_format;
    char *format;
    bool has_mode;
    NewImageMode mode;
};

void qapi_free_BlockdevSnapshotList(BlockdevSnapshotList *obj);
void qapi_free_BlockdevSnapshot(BlockdevSnapshot *obj);

struct DriveBackup
{
    char *device;
    char *target;
    bool has_format;
    char *format;
    MirrorSyncMode sync;
    bool has_mode;
    NewImageMode mode;
    bool has_speed;
    int64_t speed;
    bool has_on_source_error;
    BlockdevOnError on_source_error;
    bool has_on_target_error;
    BlockdevOnError on_target_error;
};

void qapi_free_DriveBackupList(DriveBackupList *obj);
void qapi_free_DriveBackup(DriveBackup *obj);

struct BlockdevBackup
{
    char *device;
    char *target;
    MirrorSyncMode sync;
    bool has_speed;
    int64_t speed;
    bool has_on_source_error;
    BlockdevOnError on_source_error;
    bool has_on_target_error;
    BlockdevOnError on_target_error;
};

void qapi_free_BlockdevBackupList(BlockdevBackupList *obj);
void qapi_free_BlockdevBackup(BlockdevBackup *obj);

void qapi_free_BlockdevDiscardOptionsList(BlockdevDiscardOptionsList *obj);

void qapi_free_BlockdevDetectZeroesOptionsList(BlockdevDetectZeroesOptionsList *obj);

void qapi_free_BlockdevAioOptionsList(BlockdevAioOptionsList *obj);

struct BlockdevCacheOptions
{
    bool has_writeback;
    bool writeback;
    bool has_direct;
    bool direct;
    bool has_no_flush;
    bool no_flush;
};

void qapi_free_BlockdevCacheOptionsList(BlockdevCacheOptionsList *obj);
void qapi_free_BlockdevCacheOptions(BlockdevCacheOptions *obj);

void qapi_free_BlockdevDriverList(BlockdevDriverList *obj);

struct BlockdevOptionsBase
{
    BlockdevDriver driver;
    bool has_id;
    char *id;
    bool has_node_name;
    char *node_name;
    bool has_discard;
    BlockdevDiscardOptions discard;
    bool has_cache;
    BlockdevCacheOptions *cache;
    bool has_aio;
    BlockdevAioOptions aio;
    bool has_rerror;
    BlockdevOnError rerror;
    bool has_werror;
    BlockdevOnError werror;
    bool has_read_only;
    bool read_only;
    bool has_detect_zeroes;
    BlockdevDetectZeroesOptions detect_zeroes;
};

void qapi_free_BlockdevOptionsBaseList(BlockdevOptionsBaseList *obj);
void qapi_free_BlockdevOptionsBase(BlockdevOptionsBase *obj);

struct BlockdevOptionsFile
{
    char *filename;
};

void qapi_free_BlockdevOptionsFileList(BlockdevOptionsFileList *obj);
void qapi_free_BlockdevOptionsFile(BlockdevOptionsFile *obj);

struct BlockdevOptionsNull
{
    bool has_size;
    int64_t size;
};

void qapi_free_BlockdevOptionsNullList(BlockdevOptionsNullList *obj);
void qapi_free_BlockdevOptionsNull(BlockdevOptionsNull *obj);

struct BlockdevOptionsVVFAT
{
    char *dir;
    bool has_fat_type;
    int64_t fat_type;
    bool has_floppy;
    bool floppy;
    bool has_rw;
    bool rw;
};

void qapi_free_BlockdevOptionsVVFATList(BlockdevOptionsVVFATList *obj);
void qapi_free_BlockdevOptionsVVFAT(BlockdevOptionsVVFAT *obj);

struct BlockdevOptionsGenericFormat
{
    BlockdevRef *file;
};

void qapi_free_BlockdevOptionsGenericFormatList(BlockdevOptionsGenericFormatList *obj);
void qapi_free_BlockdevOptionsGenericFormat(BlockdevOptionsGenericFormat *obj);

struct BlockdevOptionsGenericCOWFormat
{
    BlockdevOptionsGenericFormat *base;
    bool has_backing;
    BlockdevRef *backing;
};

void qapi_free_BlockdevOptionsGenericCOWFormatList(BlockdevOptionsGenericCOWFormatList *obj);
void qapi_free_BlockdevOptionsGenericCOWFormat(BlockdevOptionsGenericCOWFormat *obj);

void qapi_free_Qcow2OverlapCheckModeList(Qcow2OverlapCheckModeList *obj);

struct Qcow2OverlapCheckFlags
{
    bool has_q_template;
    Qcow2OverlapCheckMode q_template;
    bool has_main_header;
    bool main_header;
    bool has_active_l1;
    bool active_l1;
    bool has_active_l2;
    bool active_l2;
    bool has_refcount_table;
    bool refcount_table;
    bool has_refcount_block;
    bool refcount_block;
    bool has_snapshot_table;
    bool snapshot_table;
    bool has_inactive_l1;
    bool inactive_l1;
    bool has_inactive_l2;
    bool inactive_l2;
};

void qapi_free_Qcow2OverlapCheckFlagsList(Qcow2OverlapCheckFlagsList *obj);
void qapi_free_Qcow2OverlapCheckFlags(Qcow2OverlapCheckFlags *obj);

struct Qcow2OverlapChecks
{
    Qcow2OverlapChecksKind kind;
    union {
        void *data;
        Qcow2OverlapCheckFlags *flags;
        Qcow2OverlapCheckMode mode;
    };
};
extern const int Qcow2OverlapChecks_qtypes[];
void qapi_free_Qcow2OverlapChecksList(Qcow2OverlapChecksList *obj);
void qapi_free_Qcow2OverlapChecks(Qcow2OverlapChecks *obj);

struct BlockdevOptionsQcow2
{
    BlockdevOptionsGenericCOWFormat *base;
    bool has_lazy_refcounts;
    bool lazy_refcounts;
    bool has_pass_discard_request;
    bool pass_discard_request;
    bool has_pass_discard_snapshot;
    bool pass_discard_snapshot;
    bool has_pass_discard_other;
    bool pass_discard_other;
    bool has_overlap_check;
    Qcow2OverlapChecks *overlap_check;
    bool has_cache_size;
    int64_t cache_size;
    bool has_l2_cache_size;
    int64_t l2_cache_size;
    bool has_refcount_cache_size;
    int64_t refcount_cache_size;
};

void qapi_free_BlockdevOptionsQcow2List(BlockdevOptionsQcow2List *obj);
void qapi_free_BlockdevOptionsQcow2(BlockdevOptionsQcow2 *obj);

struct BlockdevOptionsArchipelago
{
    char *volume;
    bool has_mport;
    int64_t mport;
    bool has_vport;
    int64_t vport;
    bool has_segment;
    char *segment;
};

void qapi_free_BlockdevOptionsArchipelagoList(BlockdevOptionsArchipelagoList *obj);
void qapi_free_BlockdevOptionsArchipelago(BlockdevOptionsArchipelago *obj);

void qapi_free_BlkdebugEventList(BlkdebugEventList *obj);

struct BlkdebugInjectErrorOptions
{
    BlkdebugEvent event;
    bool has_state;
    int64_t state;
    bool has_q_errno;
    int64_t q_errno;
    bool has_sector;
    int64_t sector;
    bool has_once;
    bool once;
    bool has_immediately;
    bool immediately;
};

void qapi_free_BlkdebugInjectErrorOptionsList(BlkdebugInjectErrorOptionsList *obj);
void qapi_free_BlkdebugInjectErrorOptions(BlkdebugInjectErrorOptions *obj);

struct BlkdebugSetStateOptions
{
    BlkdebugEvent event;
    bool has_state;
    int64_t state;
    int64_t new_state;
};

void qapi_free_BlkdebugSetStateOptionsList(BlkdebugSetStateOptionsList *obj);
void qapi_free_BlkdebugSetStateOptions(BlkdebugSetStateOptions *obj);

struct BlockdevOptionsBlkdebug
{
    BlockdevRef *image;
    bool has_config;
    char *config;
    bool has_align;
    int64_t align;
    bool has_inject_error;
    BlkdebugInjectErrorOptionsList *inject_error;
    bool has_set_state;
    BlkdebugSetStateOptionsList *set_state;
};

void qapi_free_BlockdevOptionsBlkdebugList(BlockdevOptionsBlkdebugList *obj);
void qapi_free_BlockdevOptionsBlkdebug(BlockdevOptionsBlkdebug *obj);

struct BlockdevOptionsBlkverify
{
    BlockdevRef *test;
    BlockdevRef *raw;
};

void qapi_free_BlockdevOptionsBlkverifyList(BlockdevOptionsBlkverifyList *obj);
void qapi_free_BlockdevOptionsBlkverify(BlockdevOptionsBlkverify *obj);

void qapi_free_QuorumReadPatternList(QuorumReadPatternList *obj);

struct BlockdevOptionsQuorum
{
    bool has_blkverify;
    bool blkverify;
    BlockdevRefList *children;
    int64_t vote_threshold;
    bool has_rewrite_corrupted;
    bool rewrite_corrupted;
    bool has_read_pattern;
    QuorumReadPattern read_pattern;
};

void qapi_free_BlockdevOptionsQuorumList(BlockdevOptionsQuorumList *obj);
void qapi_free_BlockdevOptionsQuorum(BlockdevOptionsQuorum *obj);

struct BlockdevOptions
{
    BlockdevDriver kind;
    union {
        void *data;
        BlockdevOptionsArchipelago *archipelago;
        BlockdevOptionsBlkdebug *blkdebug;
        BlockdevOptionsBlkverify *blkverify;
        BlockdevOptionsGenericFormat *bochs;
        BlockdevOptionsGenericFormat *cloop;
        BlockdevOptionsGenericFormat *dmg;
        BlockdevOptionsFile *file;
        BlockdevOptionsFile *ftp;
        BlockdevOptionsFile *ftps;
        BlockdevOptionsFile *host_cdrom;
        BlockdevOptionsFile *host_device;
        BlockdevOptionsFile *host_floppy;
        BlockdevOptionsFile *http;
        BlockdevOptionsFile *https;
        BlockdevOptionsNull *null_aio;
        BlockdevOptionsNull *null_co;
        BlockdevOptionsGenericFormat *parallels;
        BlockdevOptionsQcow2 *qcow2;
        BlockdevOptionsGenericCOWFormat *qcow;
        BlockdevOptionsGenericCOWFormat *qed;
        BlockdevOptionsQuorum *quorum;
        BlockdevOptionsGenericFormat *raw;
        BlockdevOptionsFile *tftp;
        BlockdevOptionsGenericFormat *vdi;
        BlockdevOptionsGenericFormat *vhdx;
        BlockdevOptionsGenericCOWFormat *vmdk;
        BlockdevOptionsGenericFormat *vpc;
        BlockdevOptionsVVFAT *vvfat;
    };
    bool has_id;
    char *id;
    bool has_node_name;
    char *node_name;
    bool has_discard;
    BlockdevDiscardOptions discard;
    bool has_cache;
    BlockdevCacheOptions *cache;
    bool has_aio;
    BlockdevAioOptions aio;
    bool has_rerror;
    BlockdevOnError rerror;
    bool has_werror;
    BlockdevOnError werror;
    bool has_read_only;
    bool read_only;
    bool has_detect_zeroes;
    BlockdevDetectZeroesOptions detect_zeroes;
};
void qapi_free_BlockdevOptionsList(BlockdevOptionsList *obj);
void qapi_free_BlockdevOptions(BlockdevOptions *obj);

struct BlockdevRef
{
    BlockdevRefKind kind;
    union {
        void *data;
        BlockdevOptions *definition;
        char *reference;
    };
};
extern const int BlockdevRef_qtypes[];
void qapi_free_BlockdevRefList(BlockdevRefList *obj);
void qapi_free_BlockdevRef(BlockdevRef *obj);

void qapi_free_BlockErrorActionList(BlockErrorActionList *obj);

void qapi_free_PreallocModeList(PreallocModeList *obj);

void qapi_free_BiosAtaTranslationList(BiosAtaTranslationList *obj);

struct BlockdevSnapshotInternal
{
    char *device;
    char *name;
};

void qapi_free_BlockdevSnapshotInternalList(BlockdevSnapshotInternalList *obj);
void qapi_free_BlockdevSnapshotInternal(BlockdevSnapshotInternal *obj);

void qapi_free_TraceEventStateList(TraceEventStateList *obj);

struct TraceEventInfo
{
    char *name;
    TraceEventState state;
};

void qapi_free_TraceEventInfoList(TraceEventInfoList *obj);
void qapi_free_TraceEventInfo(TraceEventInfo *obj);

void qapi_free_LostTickPolicyList(LostTickPolicyList *obj);

struct NameInfo
{
    bool has_name;
    char *name;
};

void qapi_free_NameInfoList(NameInfoList *obj);
void qapi_free_NameInfo(NameInfo *obj);

struct VmxInfo
{
    bool enabled;
    bool present;
};

void qapi_free_VmxInfoList(VmxInfoList *obj);
void qapi_free_VmxInfo(VmxInfo *obj);

void qapi_free_RunStateList(RunStateList *obj);

struct StatusInfo
{
    bool running;
    bool singlestep;
    RunState status;
};

void qapi_free_StatusInfoList(StatusInfoList *obj);
void qapi_free_StatusInfo(StatusInfo *obj);

struct UuidInfo
{
    char *UUID;
};

void qapi_free_UuidInfoList(UuidInfoList *obj);
void qapi_free_UuidInfo(UuidInfo *obj);

struct ChardevInfo
{
    char *label;
    char *filename;
    bool frontend_open;
};

void qapi_free_ChardevInfoList(ChardevInfoList *obj);
void qapi_free_ChardevInfo(ChardevInfo *obj);

struct ChardevBackendInfo
{
    char *name;
};

void qapi_free_ChardevBackendInfoList(ChardevBackendInfoList *obj);
void qapi_free_ChardevBackendInfo(ChardevBackendInfo *obj);

void qapi_free_DataFormatList(DataFormatList *obj);

struct EventInfo
{
    char *name;
};

void qapi_free_EventInfoList(EventInfoList *obj);
void qapi_free_EventInfo(EventInfo *obj);

struct MigrationStats
{
    int64_t transferred;
    int64_t remaining;
    int64_t total;
    int64_t duplicate;
    int64_t skipped;
    int64_t normal;
    int64_t normal_bytes;
    int64_t dirty_pages_rate;
    double mbps;
    int64_t dirty_sync_count;
};

void qapi_free_MigrationStatsList(MigrationStatsList *obj);
void qapi_free_MigrationStats(MigrationStats *obj);

struct XBZRLECacheStats
{
    int64_t cache_size;
    int64_t bytes;
    int64_t pages;
    int64_t cache_miss;
    double cache_miss_rate;
    int64_t overflow;
};

void qapi_free_XBZRLECacheStatsList(XBZRLECacheStatsList *obj);
void qapi_free_XBZRLECacheStats(XBZRLECacheStats *obj);

struct MigrationInfo
{
    bool has_status;
    char *status;
    bool has_ram;
    MigrationStats *ram;
    bool has_disk;
    MigrationStats *disk;
    bool has_xbzrle_cache;
    XBZRLECacheStats *xbzrle_cache;
    bool has_total_time;
    int64_t total_time;
    bool has_expected_downtime;
    int64_t expected_downtime;
    bool has_downtime;
    int64_t downtime;
    bool has_setup_time;
    int64_t setup_time;
};

void qapi_free_MigrationInfoList(MigrationInfoList *obj);
void qapi_free_MigrationInfo(MigrationInfo *obj);

void qapi_free_MigrationCapabilityList(MigrationCapabilityList *obj);

struct MigrationCapabilityStatus
{
    MigrationCapability capability;
    bool state;
};

void qapi_free_MigrationCapabilityStatusList(MigrationCapabilityStatusList *obj);
void qapi_free_MigrationCapabilityStatus(MigrationCapabilityStatus *obj);

struct MouseInfo
{
    char *name;
    int64_t index;
    bool current;
    bool absolute;
};

void qapi_free_MouseInfoList(MouseInfoList *obj);
void qapi_free_MouseInfo(MouseInfo *obj);

struct CpuInfo
{
    int64_t GETCPU;
    bool current;
    bool halted;
    bool has_pc;
    int64_t pc;
    bool has_nip;
    int64_t nip;
    bool has_npc;
    int64_t npc;
    bool has_PC;
    int64_t PC;
    int64_t thread_id;
};

void qapi_free_CpuInfoList(CpuInfoList *obj);
void qapi_free_CpuInfo(CpuInfo *obj);

struct IOThreadInfo
{
    char *id;
    int64_t thread_id;
};

void qapi_free_IOThreadInfoList(IOThreadInfoList *obj);
void qapi_free_IOThreadInfo(IOThreadInfo *obj);

void qapi_free_NetworkAddressFamilyList(NetworkAddressFamilyList *obj);

struct SpiceBasicInfo
{
    char *host;
    char *port;
    NetworkAddressFamily family;
};

void qapi_free_SpiceBasicInfoList(SpiceBasicInfoList *obj);
void qapi_free_SpiceBasicInfo(SpiceBasicInfo *obj);

struct SpiceServerInfo
{
    SpiceBasicInfo *base;
    bool has_auth;
    char *auth;
};

void qapi_free_SpiceServerInfoList(SpiceServerInfoList *obj);
void qapi_free_SpiceServerInfo(SpiceServerInfo *obj);

struct SpiceChannel
{
    SpiceBasicInfo *base;
    int64_t connection_id;
    int64_t channel_type;
    int64_t channel_id;
    bool tls;
};

void qapi_free_SpiceChannelList(SpiceChannelList *obj);
void qapi_free_SpiceChannel(SpiceChannel *obj);

void qapi_free_SpiceQueryMouseModeList(SpiceQueryMouseModeList *obj);

struct SpiceInfo
{
    bool enabled;
    bool migrated;
    bool has_host;
    char *host;
    bool has_port;
    int64_t port;
    bool has_tls_port;
    int64_t tls_port;
    bool has_auth;
    char *auth;
    bool has_compiled_version;
    char *compiled_version;
    SpiceQueryMouseMode mouse_mode;
    bool has_channels;
    SpiceChannelList *channels;
};

void qapi_free_SpiceInfoList(SpiceInfoList *obj);
void qapi_free_SpiceInfo(SpiceInfo *obj);

struct BalloonInfo
{
    int64_t actual;
};

void qapi_free_BalloonInfoList(BalloonInfoList *obj);
void qapi_free_BalloonInfo(BalloonInfo *obj);

struct PciMemoryRange
{
    int64_t base;
    int64_t limit;
};

void qapi_free_PciMemoryRangeList(PciMemoryRangeList *obj);
void qapi_free_PciMemoryRange(PciMemoryRange *obj);

struct PciMemoryRegion
{
    int64_t bar;
    char *type;
    int64_t address;
    int64_t size;
    bool has_prefetch;
    bool prefetch;
    bool has_mem_type_64;
    bool mem_type_64;
};

void qapi_free_PciMemoryRegionList(PciMemoryRegionList *obj);
void qapi_free_PciMemoryRegion(PciMemoryRegion *obj);

struct PciBridgeInfo
{
    struct 
    {
        int64_t number;
        int64_t secondary;
        int64_t subordinate;
        PciMemoryRange *io_range;
        PciMemoryRange *memory_range;
        PciMemoryRange *prefetchable_range;
    } bus;
    bool has_devices;
    PciDeviceInfoList *devices;
};

void qapi_free_PciBridgeInfoList(PciBridgeInfoList *obj);
void qapi_free_PciBridgeInfo(PciBridgeInfo *obj);

struct PciDeviceInfo
{
    int64_t bus;
    int64_t slot;
    int64_t function;
    struct 
    {
        bool has_desc;
        char *desc;
        int64_t q_class;
    } class_info;
    struct 
    {
        int64_t device;
        int64_t vendor;
    } id;
    bool has_irq;
    int64_t irq;
    char *qdev_id;
    bool has_pci_bridge;
    PciBridgeInfo *pci_bridge;
    PciMemoryRegionList *regions;
};

void qapi_free_PciDeviceInfoList(PciDeviceInfoList *obj);
void qapi_free_PciDeviceInfo(PciDeviceInfo *obj);

struct PciInfo
{
    int64_t bus;
    PciDeviceInfoList *devices;
};

void qapi_free_PciInfoList(PciInfoList *obj);
void qapi_free_PciInfo(PciInfo *obj);

struct Abort
{
};

void qapi_free_AbortList(AbortList *obj);
void qapi_free_Abort(Abort *obj);

struct TransactionAction
{
    TransactionActionKind kind;
    union {
        void *data;
        BlockdevSnapshot *blockdev_snapshot_sync;
        DriveBackup *drive_backup;
        BlockdevBackup *blockdev_backup;
        Abort *abort;
        BlockdevSnapshotInternal *blockdev_snapshot_internal_sync;
    };
};
void qapi_free_TransactionActionList(TransactionActionList *obj);
void qapi_free_TransactionAction(TransactionAction *obj);

struct OProInfo
{
    char *name;
    char *type;
};

void qapi_free_ObjectPropertyInfoList(ObjectPropertyInfoList *obj);
void qapi_free_ObjectPropertyInfo(OProInfo *obj);

struct OTypeInfo
{
    char *name;
};

void qapi_free_ObjectTypeInfoList(ObjectTypeInfoList *obj);
void qapi_free_ObjectTypeInfo(OTypeInfo *obj);

struct DevicePropertyInfo
{
    char *name;
    char *type;
    bool has_description;
    char *description;
};

void qapi_free_DevicePropertyInfoList(DevicePropertyInfoList *obj);
void qapi_free_DevicePropertyInfo(DevicePropertyInfo *obj);

void qapi_free_DumpGuestMemoryFormatList(DumpGuestMemoryFormatList *obj);

struct DumpGuestMemoryCapability
{
    DumpGuestMemoryFormatList *formats;
};

void qapi_free_DumpGuestMemoryCapabilityList(DumpGuestMemoryCapabilityList *obj);
void qapi_free_DumpGuestMemoryCapability(DumpGuestMemoryCapability *obj);

struct NetdevNoneOptions
{
};

void qapi_free_NetdevNoneOptionsList(NetdevNoneOptionsList *obj);
void qapi_free_NetdevNoneOptions(NetdevNoneOptions *obj);

struct NetLegacyNicOptions
{
    bool has_netdev;
    char *netdev;
    bool has_macaddr;
    char *macaddr;
    bool has_model;
    char *model;
    bool has_addr;
    char *addr;
    bool has_vectors;
    uint32_t vectors;
};

void qapi_free_NetLegacyNicOptionsList(NetLegacyNicOptionsList *obj);
void qapi_free_NetLegacyNicOptions(NetLegacyNicOptions *obj);

struct String
{
    char *str;
};

void qapi_free_StringList(StringList *obj);
void qapi_free_String(String *obj);

struct NetdevUserOptions
{
    bool has_hostname;
    char *hostname;
    bool has_q_restrict;
    bool q_restrict;
    bool has_ip;
    char *ip;
    bool has_net;
    char *net;
    bool has_host;
    char *host;
    bool has_tftp;
    char *tftp;
    bool has_bootfile;
    char *bootfile;
    bool has_dhcpstart;
    char *dhcpstart;
    bool has_dns;
    char *dns;
    bool has_dnssearch;
    StringList *dnssearch;
    bool has_smb;
    char *smb;
    bool has_smbserver;
    char *smbserver;
    bool has_hostfwd;
    StringList *hostfwd;
    bool has_guestfwd;
    StringList *guestfwd;
};

void qapi_free_NetdevUserOptionsList(NetdevUserOptionsList *obj);
void qapi_free_NetdevUserOptions(NetdevUserOptions *obj);

struct NetdevTapOptions
{
    bool has_ifname;
    char *ifname;
    bool has_fd;
    char *fd;
    bool has_fds;
    char *fds;
    bool has_script;
    char *script;
    bool has_downscript;
    char *downscript;
    bool has_helper;
    char *helper;
    bool has_sndbuf;
    uint64_t sndbuf;
    bool has_vnet_hdr;
    bool vnet_hdr;
    bool has_vhost;
    bool vhost;
    bool has_vhostfd;
    char *vhostfd;
    bool has_vhostfds;
    char *vhostfds;
    bool has_vhostforce;
    bool vhostforce;
    bool has_queues;
    uint32_t queues;
    bool has_bridge;
    char* bridge;
};

void qapi_free_NetdevTapOptionsList(NetdevTapOptionsList *obj);
void qapi_free_NetdevTapOptions(NetdevTapOptions *obj);

struct NetdevSocketOptions
{
    bool has_fd;
    char *fd;
    bool has_listen;
    char *listen;
    bool has_connect;
    char *connect;
    bool has_mcast;
    char *mcast;
    bool has_localaddr;
    char *localaddr;
    bool has_udp;
    char *udp;
};

void qapi_free_NetdevSocketOptionsList(NetdevSocketOptionsList *obj);
void qapi_free_NetdevSocketOptions(NetdevSocketOptions *obj);

struct NetdevL2TPv3Options
{
    char *src;
    char *dst;
    bool has_srcport;
    char *srcport;
    bool has_dstport;
    char *dstport;
    bool has_ipv6;
    bool ipv6;
    bool has_udp;
    bool udp;
    bool has_cookie64;
    bool cookie64;
    bool has_counter;
    bool counter;
    bool has_pincounter;
    bool pincounter;
    bool has_txcookie;
    uint64_t txcookie;
    bool has_rxcookie;
    uint64_t rxcookie;
    uint32_t txsession;
    bool has_rxsession;
    uint32_t rxsession;
    bool has_offset;
    uint32_t offset;
};

void qapi_free_NetdevL2TPv3OptionsList(NetdevL2TPv3OptionsList *obj);
void qapi_free_NetdevL2TPv3Options(NetdevL2TPv3Options *obj);

struct NetdevVdeOptions
{
    bool has_sock;
    char *sock;
    bool has_port;
    uint16_t port;
    bool has_group;
    char *group;
    bool has_mode;
    uint16_t mode;
};

void qapi_free_NetdevVdeOptionsList(NetdevVdeOptionsList *obj);
void qapi_free_NetdevVdeOptions(NetdevVdeOptions *obj);

struct NetdevDumpOptions
{
    bool has_len;
    uint64_t len;
    bool has_file;
    char *file;
};

void qapi_free_NetdevDumpOptionsList(NetdevDumpOptionsList *obj);
void qapi_free_NetdevDumpOptions(NetdevDumpOptions *obj);

struct NetdevBridgeOptions
{
    bool has_br;
    char *br;
    bool has_helper;
    char *helper;
};

void qapi_free_NetdevBridgeOptionsList(NetdevBridgeOptionsList *obj);
void qapi_free_NetdevBridgeOptions(NetdevBridgeOptions *obj);

struct NetdevHubPortOptions
{
    int32_t hubid;
};

void qapi_free_NetdevHubPortOptionsList(NetdevHubPortOptionsList *obj);
void qapi_free_NetdevHubPortOptions(NetdevHubPortOptions *obj);

struct NetdevNetmapOptions
{
    char *ifname;
    bool has_devname;
    char *devname;
};

void qapi_free_NetdevNetmapOptionsList(NetdevNetmapOptionsList *obj);
void qapi_free_NetdevNetmapOptions(NetdevNetmapOptions *obj);

struct NetdevVhostUserOptions
{
    char *chardev;
    bool has_vhostforce;
    bool vhostforce;
};

struct NetdevVnetOptions
{
    bool has_mode;
    char *mode;
    bool has_uuid;
    char *uuid;
};

void qapi_free_NetdevVhostUserOptionsList(NetdevVhostUserOptionsList *obj);
void qapi_free_NetdevVhostUserOptions(NetdevVhostUserOptions *obj);

struct NetClientOptions
{
    NetClientOptionsKind kind;
    union {
        void *data;
        NetdevNoneOptions *none;
        NetLegacyNicOptions *nic;
        NetdevUserOptions *user;
        NetdevTapOptions *tap;
        NetdevL2TPv3Options *l2tpv3;
        NetdevSocketOptions *socket;
        NetdevVdeOptions *vde;
        NetdevDumpOptions *dump;
        NetdevBridgeOptions *bridge;
        NetdevHubPortOptions *hubport;
        NetdevNetmapOptions *netmap;
        NetdevVhostUserOptions *vhost_user;
        NetdevVnetOptions *vnet;
    };
};
void qapi_free_NetClientOptionsList(NetClientOptionsList *obj);
void qapi_free_NetClientOptions(NetClientOptions *obj);

struct NetLegacy
{
    bool has_vlan;
    int32_t vlan;
    bool has_id;
    char *id;
    bool has_name;
    char *name;
    NetClientOptions *opts;
};

void qapi_free_NetLegacyList(NetLegacyList *obj);
void qapi_free_NetLegacy(NetLegacy *obj);

struct Netdev
{
    char *id;
    NetClientOptions *opts;
};

void qapi_free_NetdevList(NetdevList *obj);
void qapi_free_Netdev(Netdev *obj);

struct InetSocketAddress
{
    char *host;
    char *port;
    bool has_to;
    uint16_t to;
    bool has_ipv4;
    bool ipv4;
    bool has_ipv6;
    bool ipv6;
};

void qapi_free_InetSocketAddressList(InetSocketAddressList *obj);
void qapi_free_InetSocketAddress(InetSocketAddress *obj);

struct UnixSocketAddress
{
    char *path;
};

void qapi_free_UnixSocketAddressList(UnixSocketAddressList *obj);
void qapi_free_UnixSocketAddress(UnixSocketAddress *obj);

struct SocketAddress
{
    SocketAddressKind kind;
    union {
        void *data;
        InetSocketAddress *inet;
        UnixSocketAddress *q_unix;
        String *fd;
    };
};
void qapi_free_SocketAddressList(SocketAddressList *obj);
void qapi_free_SocketAddress(SocketAddress *obj);

struct MachineInfo
{
    char *name;
    bool has_alias;
    char *alias;
    bool has_is_default;
    bool is_default;
    int64_t cpu_max;
};

void qapi_free_MachineInfoList(MachineInfoList *obj);
void qapi_free_MachineInfo(MachineInfo *obj);

struct CpuDefinitionInfo
{
    char *name;
};

void qapi_free_CpuDefinitionInfoList(CpuDefinitionInfoList *obj);
void qapi_free_CpuDefinitionInfo(CpuDefinitionInfo *obj);

struct AddfdInfo
{
    int64_t fdset_id;
    int64_t fd;
};

void qapi_free_AddfdInfoList(AddfdInfoList *obj);
void qapi_free_AddfdInfo(AddfdInfo *obj);

struct FdsetFdInfo
{
    int64_t fd;
    bool has_opaque;
    char *opaque;
};

void qapi_free_FdsetFdInfoList(FdsetFdInfoList *obj);
void qapi_free_FdsetFdInfo(FdsetFdInfo *obj);

struct FdsetInfo
{
    int64_t fdset_id;
    FdsetFdInfoList *fds;
};

void qapi_free_FdsetInfoList(FdsetInfoList *obj);
void qapi_free_FdsetInfo(FdsetInfo *obj);

struct TargetInfo
{
    char *arch;
};

void qapi_free_TargetInfoList(TargetInfoList *obj);
void qapi_free_TargetInfo(TargetInfo *obj);

void qapi_free_QKeyCodeList(QKeyCodeList *obj);

struct KeyValue
{
    KeyValueKind kind;
    union {
        void *data;
        int64_t number;
        QKeyCode qcode;
    };
};
void qapi_free_KeyValueList(KeyValueList *obj);
void qapi_free_KeyValue(KeyValue *obj);

struct ChardevFile
{
    bool has_in;
    char *in;
    char *out;
};

void qapi_free_ChardevFileList(ChardevFileList *obj);
void qapi_free_ChardevFile(ChardevFile *obj);

struct ChardevHostdev
{
    char *device;
};

void qapi_free_ChardevHostdevList(ChardevHostdevList *obj);
void qapi_free_ChardevHostdev(ChardevHostdev *obj);

struct ChardevSocket
{
    SocketAddress *addr;
    bool has_server;
    bool server;
    bool has_wait;
    bool wait;
    bool has_nodelay;
    bool nodelay;
    bool has_telnet;
    bool telnet;
    bool has_reconnect;
    int64_t reconnect;
};

void qapi_free_ChardevSocketList(ChardevSocketList *obj);
void qapi_free_ChardevSocket(ChardevSocket *obj);

struct ChardevUdp
{
    SocketAddress *remote;
    bool has_local;
    SocketAddress *local;
};

void qapi_free_ChardevUdpList(ChardevUdpList *obj);
void qapi_free_ChardevUdp(ChardevUdp *obj);

struct ChardevMux
{
    char *chardev;
};

void qapi_free_ChardevMuxList(ChardevMuxList *obj);
void qapi_free_ChardevMux(ChardevMux *obj);

struct ChardevStdio
{
    bool has_signal;
    bool signal;
};

void qapi_free_ChardevStdioList(ChardevStdioList *obj);
void qapi_free_ChardevStdio(ChardevStdio *obj);

struct ChardevSpiceChannel
{
    char *type;
};

void qapi_free_ChardevSpiceChannelList(ChardevSpiceChannelList *obj);
void qapi_free_ChardevSpiceChannel(ChardevSpiceChannel *obj);

struct ChardevSpicePort
{
    char *fqdn;
};

void qapi_free_ChardevSpicePortList(ChardevSpicePortList *obj);
void qapi_free_ChardevSpicePort(ChardevSpicePort *obj);

struct ChardevVC
{
    bool has_width;
    int64_t width;
    bool has_height;
    int64_t height;
    bool has_cols;
    int64_t cols;
    bool has_rows;
    int64_t rows;
};

void qapi_free_ChardevVCList(ChardevVCList *obj);
void qapi_free_ChardevVC(ChardevVC *obj);

struct ChardevRingbuf
{
    bool has_size;
    int64_t size;
};

void qapi_free_ChardevRingbufList(ChardevRingbufList *obj);
void qapi_free_ChardevRingbuf(ChardevRingbuf *obj);

struct ChardevDummy
{
};

void qapi_free_ChardevDummyList(ChardevDummyList *obj);
void qapi_free_ChardevDummy(ChardevDummy *obj);

struct ChardevBackend
{
    ChardevBackendKind kind;
    union {
        void *data;
        ChardevFile *file;
        ChardevHostdev *serial;
        ChardevHostdev *parallel;
        ChardevHostdev *pipe;
        ChardevSocket *socket;
        ChardevUdp *udp;
        ChardevDummy *pty;
        ChardevDummy *null;
        ChardevMux *mux;
        ChardevDummy *msmouse;
        ChardevDummy *braille;
        ChardevDummy *testdev;
        ChardevStdio *stdio;
        ChardevDummy *console;
        ChardevSpiceChannel *spicevmc;
        ChardevSpicePort *spiceport;
        ChardevVC *vc;
        ChardevRingbuf *ringbuf;
        ChardevRingbuf *memory;
    };
};
void qapi_free_ChardevBackendList(ChardevBackendList *obj);
void qapi_free_ChardevBackend(ChardevBackend *obj);

struct ChardevReturn
{
    bool has_pty;
    char *pty;
};

void qapi_free_ChardevReturnList(ChardevReturnList *obj);
void qapi_free_ChardevReturn(ChardevReturn *obj);

void qapi_free_TpmModelList(TpmModelList *obj);

void qapi_free_TpmTypeList(TpmTypeList *obj);

struct TPMPassthroughOptions
{
    bool has_path;
    char *path;
    bool has_cancel_path;
    char *cancel_path;
};

void qapi_free_TPMPassthroughOptionsList(TPMPassthroughOptionsList *obj);
void qapi_free_TPMPassthroughOptions(TPMPassthroughOptions *obj);

struct TpmTypeOptions
{
    TpmTypeOptionsKind kind;
    union {
        void *data;
        TPMPassthroughOptions *passthrough;
    };
};
void qapi_free_TpmTypeOptionsList(TpmTypeOptionsList *obj);
void qapi_free_TpmTypeOptions(TpmTypeOptions *obj);

struct TPMInfo
{
    char *id;
    TpmModel model;
    TpmTypeOptions *options;
};

void qapi_free_TPMInfoList(TPMInfoList *obj);
void qapi_free_TPMInfo(TPMInfo *obj);

struct AcpiTableOptions
{
    bool has_sig;
    char *sig;
    bool has_rev;
    uint8_t rev;
    bool has_oem_id;
    char *oem_id;
    bool has_oem_table_id;
    char *oem_table_id;
    bool has_oem_rev;
    uint32_t oem_rev;
    bool has_asl_compiler_id;
    char *asl_compiler_id;
    bool has_asl_compiler_rev;
    uint32_t asl_compiler_rev;
    bool has_file;
    char *file;
    bool has_data;
    char *data;
};

void qapi_free_AcpiTableOptionsList(AcpiTableOptionsList *obj);
void qapi_free_AcpiTableOptions(AcpiTableOptions *obj);

void qapi_free_CommandLineParameterTypeList(CommandLineParameterTypeList *obj);

struct CommandLineParameterInfo
{
    char *name;
    CommandLineParameterType type;
    bool has_help;
    char *help;
    bool has_q_default;
    char *q_default;
};

void qapi_free_CommandLineParameterInfoList(CommandLineParameterInfoList *obj);
void qapi_free_CommandLineParameterInfo(CommandLineParameterInfo *obj);

struct CommandLineOptionInfo
{
    char *option;
    CommandLineParameterInfoList *parameters;
};

void qapi_free_CommandLineOptionInfoList(CommandLineOptionInfoList *obj);
void qapi_free_CommandLineOptionInfo(CommandLineOptionInfo *obj);

void qapi_free_X86CPURegister32List(X86CPURegister32List *obj);

struct X86CPUFeatureWordInfo
{
    int64_t cpuid_input_eax;
    bool has_cpuid_input_ecx;
    int64_t cpuid_input_ecx;
    X86CPURegister32 cpuid_register;
    int64_t features;
};

void qapi_free_X86CPUFeatureWordInfoList(X86CPUFeatureWordInfoList *obj);
void qapi_free_X86CPUFeatureWordInfo(X86CPUFeatureWordInfo *obj);

void qapi_free_RxStateList(RxStateList *obj);

struct RxFilterInfo
{
    char *name;
    bool promiscuous;
    RxState multicast;
    RxState unicast;
    RxState vlan;
    bool broadcast_allowed;
    bool multicast_overflow;
    bool unicast_overflow;
    char *main_mac;
    intList *vlan_table;
    strList *unicast_table;
    strList *multicast_table;
};

void qapi_free_RxFilterInfoList(RxFilterInfoList *obj);
void qapi_free_RxFilterInfo(RxFilterInfo *obj);

void qapi_free_InputButtonList(InputButtonList *obj);

void qapi_free_InputAxisList(InputAxisList *obj);

struct InputKeyEvent
{
    KeyValue *key;
    bool down;
};

void qapi_free_InputKeyEventList(InputKeyEventList *obj);
void qapi_free_InputKeyEvent(InputKeyEvent *obj);

struct InputBtnEvent
{
    InputButton button;
    bool down;
};

void qapi_free_InputBtnEventList(InputBtnEventList *obj);
void qapi_free_InputBtnEvent(InputBtnEvent *obj);

struct InputMoveEvent
{
    InputAxis axis;
    int64_t value;
};

void qapi_free_InputMoveEventList(InputMoveEventList *obj);
void qapi_free_InputMoveEvent(InputMoveEvent *obj);

struct InputEvent
{
    InputEventKind kind;
    union {
        void *data;
        InputKeyEvent *key;
        InputBtnEvent *btn;
        InputMoveEvent *rel;
        InputMoveEvent *abs;
    };
};
void qapi_free_InputEventList(InputEventList *obj);
void qapi_free_InputEvent(InputEvent *obj);

struct NumaOptions
{
    NumaOptionsKind kind;
    union {
        void *data;
        NumaNodeOptions *node;
    };
};
void qapi_free_NumaOptionsList(NumaOptionsList *obj);
void qapi_free_NumaOptions(NumaOptions *obj);

struct NumaNodeOptions
{
    bool has_nodeid;
    uint16_t nodeid;
    bool has_cpus;
    uint16List *cpus;
    bool has_mem;
    uint64_t mem;
    bool has_memdev;
    char *memdev;
};

void qapi_free_NumaNodeOptionsList(NumaNodeOptionsList *obj);
void qapi_free_NumaNodeOptions(NumaNodeOptions *obj);

void qapi_free_HostMemPolicyList(HostMemPolicyList *obj);

struct Memdev
{
    uint64_t size;
    bool merge;
    bool dump;
    bool prealloc;
    uint16List *host_nodes;
    HostMemPolicy policy;
};

void qapi_free_MemdevList(MemdevList *obj);
void qapi_free_Memdev(Memdev *obj);

struct PCDIMMDeviceInfo
{
    bool has_id;
    char *id;
    int64_t addr;
    int64_t size;
    int64_t slot;
    int64_t node;
    char *memdev;
    bool hotplugged;
    bool hotpluggable;
};

void qapi_free_PCDIMMDeviceInfoList(PCDIMMDeviceInfoList *obj);
void qapi_free_PCDIMMDeviceInfo(PCDIMMDeviceInfo *obj);

struct MemoryDeviceInfo
{
    MemoryDeviceInfoKind kind;
    union {
        void *data;
        PCDIMMDeviceInfo *dimm;
    };
};
void qapi_free_MemoryDeviceInfoList(MemoryDeviceInfoList *obj);
void qapi_free_MemoryDeviceInfo(MemoryDeviceInfo *obj);

void qapi_free_ACPISlotTypeList(ACPISlotTypeList *obj);

struct ACPIOSTInfo
{
    bool has_device;
    char *device;
    char *slot;
    ACPISlotType slot_type;
    int64_t source;
    int64_t status;
};

void qapi_free_ACPIOSTInfoList(ACPIOSTInfoList *obj);
void qapi_free_ACPIOSTInfo(ACPIOSTInfo *obj);

void qapi_free_WatchdogExpirationActionList(WatchdogExpirationActionList *obj);

void qapi_free_IoOperationTypeList(IoOperationTypeList *obj);

void qapi_free_GuestPanicActionList(GuestPanicActionList *obj);

#endif
