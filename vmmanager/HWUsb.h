//
//  HWUsb.h
//  vmx
//
//  Created by Boris Remizov on 23/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

typedef enum {
    HWUsbTypeNone   = 0,
    HWUsbTypeUSB1_0 = 0x100,
    HWUsbTypeUSB1_1 = 0x110,
    HWUsbTypeUSB2_0 = 0x200,
    HWUsbTypeUSB3_0 = 0x300
} HWUsbType;

@interface HWUsb : HWItem

/// \brief User friendly name
@property (nonatomic, copy) NSString* name;

/// \brief attributes of corresponding host device
@property (nonatomic) NSNumber* locationId;
@property (nonatomic) NSNumber* vendorId;
@property (nonatomic) NSNumber* productId;

@end
