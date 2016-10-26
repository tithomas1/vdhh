//
//  VMHW.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWAudio.h>
#import <VMManager/HWCpu.h>
#import <VMManager/HWCdrom.h>
#import <VMManager/HWDiskController.h>
#import <VMManager/HWHd.h>
#import <VMManager/HWNic.h>
#import <VMManager/HWUhc.h>
#import <VMManager/HWUsb.h>
#import <Foundation/Foundation.h>

typedef enum VMHwOptions: NSUInteger {

    VMHwRemoveUnusedProviders = 0x1
} VMHwOptions;


@interface VMHw : POD

@property HWAudio *audio;
@property NSArray<HWCdrom *> *cdrom;
@property NSString *chipset;
@property HWCpu *cpu;
@property NSArray<HWDiskController *> *disk_controller;
@property NSArray<HWHd *> *hd;
@property NSArray<HWNic *> *nic;
@property NSArray<HWUsb *> *usb;
@property NSArray<HWUhc *> *uhc;
@property NSString *ram;
@property NSString *acpi;
@property NSString *hpet;
@property NSString *hyperv;
@property NSString *vga;

- (NSNumber*)uniqueIdentifierAnongItems:(NSArray<HWItem*>*)items;

- (NSArray<HWItem*>*)clientsOf:(HWItem*)provider amongItems:(NSArray<HWItem*>*)items;
- (HWItem*)providerOf:(HWItem*)client amongItems:(NSArray<HWItem*>*)items;

@end

@interface VMHw(USB)

/// \brief Create new UHC controller with default parameters
///        The controller is already added (on success) to inventory on function return
- (HWUhc*)createNewUhc:(NSError**)error;

/// \brief Remove specified UHC controller
///        Function fails if the controller has attached clients (USB devices) and
///        force flag is not set.
- (BOOL)removeUhc:(HWUhc*)uhc force:(BOOL)force options:(NSUInteger)options error:(NSError**)error;

/// \brief Create new USB pass-through device
///        Newly created device is is automatically connected to first available
///        UHC controller. Additional UHC controller will be created if there are no
///        available controllers to handle new USB device.
- (HWUsb*)createNewUsb:(NSError**)error;

/// \brief Remove USB device from inventory
///        USB device will be detached from corresponding UHC controller
///        There is no any clients of USB pass-through devices currently,
///        so the force flag is meaningless.
- (BOOL)removeUsb:(HWUsb*)usb force:(BOOL)force options:(NSUInteger)options error:(NSError**)error;

@end
