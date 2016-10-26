//
//  VMHW.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "VMHw.h"

@implementation VMHw

// serialization metadata
+(void)_T_cdrom_T_HWCdrom{}
+(void)_T_disk_controller_T_HWDiskController{}
+(void)_T_hd_T_HWHd{}
+(void)_T_nic_T_HWNic{}
+(void)_T_uhc_T_HWUhc{}
+(void)_T_usb_T_HWUsb{}


- (NSNumber*)uniqueIdentifierAnongItems:(NSArray<HWItem*>*)items {

    if ([items count] == 0)
        return @0;

    NSUInteger min = NSUIntegerMax;
    NSUInteger max = 0;

    for (HWItem* item in items) {
        min = MIN(min, [item.id unsignedIntegerValue]);
        max = MAX(max, [item.id unsignedIntegerValue]);
    }

    if (min > 0)
        return @(min - 1);

    if (max < NSUIntegerMax)
        return @(max + 1);

    NSAssert(FALSE, @"Could not find unique identifier for sequence %@", items);
    return nil;
}

- (NSArray<HWItem*>*)clientsOf:(HWItem*)provider amongItems:(NSArray<HWItem*>*)items {
    NSAssert(provider.id, @"Unassigned provider detected %@", provider);
    NSMutableArray<HWItem*>* clients = [NSMutableArray array];
    for (HWItem* item in items) {
        NSAssert(item.id, @"Unassigned item detected %@", item);
        if ([item.provider isEqual:provider.id]) {
            NSAssert(![clients containsObject:item], @"Duplicate item detected %@", item);
            [clients addObject:item];
        }
    }
    return clients;
}

- (HWItem*)providerOf:(HWItem*)client amongItems:(NSArray<HWItem*>*)items {
    NSAssert(client.id, @"Unassigned client detected %@", client);
    for (HWItem* item in items) {
        NSAssert(item.id, @"Unassigned provider detected %@", item);
        if ([client.provider isEqual:item.id])
            return item;
    }
    return nil;
}

@end

@implementation VMHw(USB)

- (HWUhc*)createNewUhc:(NSError**)error
{
    NSError* __autoreleasing localError = nil;
    if (NULL == error)
        error = &localError;

    // TODO: control limits on UHC controllers

    HWUhc* uhc = [HWUhc new];
    uhc.model = @"usb-ehci";    // EHCI (USB 2.0) by default
    uhc.id = [self uniqueIdentifierAnongItems:self.uhc];
    if (nil == uhc.id) {
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:@{
            NSLocalizedDescriptionKey: NSLocalizedString(@"Unable to assign unique identifier to UHC", "")
        }];
        NSLog(@"Unable to assign unique identifier to UHC %@", uhc);
        return nil;
    }

    // add to the inventory
    [[self mutableArrayValueForKey:@"uhc"] addObject:uhc];
    NSLog(@"New UHC %@ added to the list", uhc);

    return uhc;
}


- (HWUhc *)createNewUsb:(NSError**)error
{
    NSError* __autoreleasing localError = nil;
    if (NULL == error)
        error = &localError;

    HWUsb* usb = [HWUsb new];

    // set unique identifier for this item
    usb.id = [self uniqueIdentifierAnongItems:self.usb];
    if (nil == usb.id) {
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:@{
            NSLocalizedDescriptionKey: NSLocalizedString(@"Unable to assign unique identifier to USB", "")
        }];
        NSLog(@"Unable to assign unique identifier to USB %@", usb);
        return nil;
    }

    // find available UHC to connect to by default
    for (HWUhc* uhc in self.uhc) {
        if ([[self clientsOf:uhc amongItems:self.usb] count] < [uhc clientsLimit]) {
            usb.provider = uhc.id;
            break;
        }
    }

    if (nil == usb.provider) {
        // No suitable UHC found, create new one
        HWUhc* uhc = [self createNewUhc:error];
        if (!uhc)
            return nil;
        usb.provider = uhc.id;
    }

    NSLog(@"New USB device created %@", usb);

    // add to inventory (possibly unconnected device)
    [[self mutableArrayValueForKey:@"usb"] addObject:usb];
    
    return usb;
}

- (BOOL)removeUhc:(HWUhc*)uhc force:(BOOL)force options:(NSUInteger)options error:(NSError**)error {

    NSError* __autoreleasing localError = nil;
    if (NULL == error)
        error = &localError;

    // find clients of the item
    NSArray* clients = [self clientsOf:uhc amongItems:self.usb];
    if (FALSE == force && [clients count] > 0) {
        NSLog(@"Device %@ has active clients attached", uhc);
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EBUSY userInfo:@{
            @"Clients": clients,
            NSLocalizedDescriptionKey: NSLocalizedString(@"Failed to remove UHC controller", ""),
            NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Device has active clients attached", ""),
            NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"Remove devices, connected to this controller first", "")
        }];
        return FALSE;
    }

    // remove clients
    for (HWUsb* usb in clients) {
        NSAssert(force, @"Inconsistency in force logic detected");
        NSLog(@"Removing client %@", usb);
        BOOL result = [self removeUsb:usb force:TRUE options:0 error:error];
        if (!result) {
            return FALSE;
        }
    }

    // remove the item itself
    NSLog(@"Removing controller %@", uhc);
    [[self mutableArrayValueForKey:@"uhc"] removeObject:uhc];

    // remove provider
    if (VMHwRemoveUnusedProviders & options && uhc.provider) {
        NSLog(@"Removing provider %u", uhc.provider);

        // currently HW doesn't specify providers for UHC, but may be
        // in future we'll support "bus=pci.0" or so...
    }

    return TRUE;
}

- (BOOL)removeUsb:(HWUsb*)item force:(BOOL)force options:(NSUInteger)options error:(NSError**)error {

    NSError* __autoreleasing localError = nil;
    if (NULL == error)
        error = &localError;

    // find clients of the item

    // remove clients
    //  currently HW doesn't support clients for USB devices, so
    //  no checks for clients attached needed

    // remove the item itself
    [[self mutableArrayValueForKey:@"usb"] removeObject:item];

    // remove provider
    if (VMHwRemoveUnusedProviders & options && item.provider) {
        NSLog(@"Removing provider %u", item.provider);

        id provider = [self providerOf:item amongItems:self.uhc];
        NSAssert(provider, @"Provider inconsistency detected");
        if (provider)
            [self removeUhc:provider force:FALSE options:options error:error];
    }

    return TRUE;
}

@end

