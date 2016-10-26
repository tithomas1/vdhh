//
//  HWUsb.h
//  vmx
//
//  Created by Boris Remizov on 19/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

@interface HWUhc : HWItem

@property (nonatomic) NSNumber* numPorts;
@property (nonatomic, copy) NSString* model;

@end
