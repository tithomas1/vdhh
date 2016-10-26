//
//  USBDeviceMonitor.h
//  vmx
//
//  Created by Boris Remizov on 31/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface USBDeviceMonitor : NSObject

@property (nonatomic, readonly, strong) NSArray* connectedDevices;

@end
