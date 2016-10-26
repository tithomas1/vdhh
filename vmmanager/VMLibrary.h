//
//  VMLibrary.h
//  vmx
//
//  Created by Alex shman on 4/7/15.
//  Copyright (c) 2015 NoName Org. All rights reserved.
//

#import <VMManager/VM.h>
#import <Foundation/Foundation.h>

@interface VMLibrary : NSObject

typedef NS_ENUM(NSUInteger, VMState) {
    VMStateUnknown,
    VMStateStopped,
    VMStateSuspended,
    VMStateStarting,
    VMStateRunning,
    VMStateSuspending,
    VMStateShuttingDown,
};


+ (instancetype) sharedVMLibrary;

- (bool) firstTimeSetup;

- (void) setVmLibraryFolder: (NSString *) vmlib_path;

- (int) maxVmMemoryMB;
- (int) minVmMemoryMB;

- (int) maxVmHDSizeMB;
- (int) minVmHDSizeMB;

- (int) maxSCSIControllers;
- (int) maxNICs;
- (int) maxHDs;
- (int) maxCDROMs;
- (int) maxUHCs;

- (NSArray*) getVmList;
- (NSString*) getVmFolder: (NSString*) name;
- (BOOL) vmExist:(NSString *) name;

- (VM *) readVmProperties: (NSString*) name;
- (void) writeVmProperties:(VM *)vm;

- (void) setVmCpuInfo: (VM*)vm cores:(NSNumber*) new_cores threads:(NSNumber*) new_threads sockets:(NSNumber*) new_sockets;
- (void) setVmMemory: (VM*)vm memory: (NSString *) ram;
- (void) setCdroms: (VM*)vm cdroms: (NSArray<HWCdrom *> *) cds;
- (void) setHds: (VM*)vm hds: (NSArray<HWHd *> *) hds;
- (void) setControllers: (VM*)vm controllers: (NSArray<HWDiskController *> *) controllers;
- (void) setNics: (VM*)vm nics: (NSArray<HWNic *> *) nics;
- (void) setAudio: (VM*)vm device: (HWAudio *) audio;
- (void) recalculateHdSize: (NSString*) name;

- (HWNic *) createNewNic: (NSString *) vm_name;
- (HWCdrom *) createNewCdrom: (NSString *) vm_name;
- (HWHd *) createNewHd: (NSString *) vm_name;
- (HWDiskController *) createNewSCSIController: (NSString *) vm_name;
- (HWAudio *) createNewAudioDevice: (NSString *) vm_name;
- (BOOL) isHdCommited: (NSString *) vm_name hd: (int) hid;
- (void) deleteHdData: (NSString *) vm_name hd: (int) hid;
- (int) countDevicesAttachedToController: (int) cid vm: (NSString *)vm_name;
- (int) countDevicesAttachedToIDE: (NSString *)vm_name;

- (void) addTemporaryVM: (NSDictionary*) props;
- (void) importTemporaryVM: (NSString *) name fromFolder: (NSString*) folder;
- (BOOL) commitTemporaryVM;
- (BOOL) isTemporaryVM: (NSString*) vm_name;
- (NSString*) renameVM: (NSString*) old_name to: new_name;
- (void) setDescriptionForVm: (NSString *) vm_name description: (NSString*) desc;
- (void) setBootDeviceForVm: (NSString *) vm_name device: (NSString*) d;
- (BOOL) deleteVm: (NSString*) vm_name;
- (void) cleanupTempVm;

- (void) setVGA: (NSString *) vm_name type: (NSString *) type;
- (void) setHeadlessMode: (NSString*) vm_name value: (BOOL) val;
- (void) setRemapCmd: (NSString*) vm_name value: (BOOL) val;
- (void) setReadOnlyVM: (NSString*) vm_name value: (BOOL) val;
- (void) setEnableFS: (NSString*) vm_name value: (BOOL) val;
- (void) setEnableCopyPaste: (NSString*) vm_name value: (BOOL) val;
- (void) setFSFolder: (NSString*) vm_name folder: (NSString*) folder;
- (void) setHDPISupport: (NSString*) vm_name value: (BOOL) val;

- (NSArray*) getOSList;
- (NSArray*) getOSFamilyList: (NSString*) os;

- (NSArray *) getSCSIModels;
- (NSString*) getSCSIDisplayName: (NSString*)model;

- (NSArray *) getNicConnTypes UNAVAILABLE_ATTRIBUTE;
- (NSArray *) getNicModels;
- (NSString*) getNicDisplayName: (NSString*)model;

- (VMState) getVmState: (NSString *) name;
- (bool) isVmRunning: (NSString *) name;
- (bool) isVmSuspending: (NSString *) name;
- (bool) isVmSuspended: (NSString *) name;
- (NSString*) getIpAddress: (NSString *) name;

- (void) shutoffVm: (NSString *) name;
- (pid_t) startVm: (NSString *) name;
- (pid_t) restartVm: (NSString *) name;
- (bool) shutdownVm: (NSString *) name;
- (bool) suspendVm: (NSString *) name;
- (bool) rebootVm: (NSString *) name;

- (void) setVmSortOrder: (NSArray*) sorted_vm_list;

- (NSString*) makeLegalVmName: (NSString*) name;
- (NSArray*) createLaunchVmParam: (NSString*) name withOptions: (VM *)vm restore: (bool) restore;

- (void) generateMacAddresses: (NSString *) vm_name;
- (void) generateUUID: (NSString *) vm_name;

// TODO: this should be private, and updated by push
- (NSString *) sendVmCommand: (NSString *) vm_name command: (NSString *)cmd;

@property (nonatomic, retain) NSString *vmLibPath;

@end

NSString *RealHomeDirectory();

NSString *RealDownloadsDirectory();

