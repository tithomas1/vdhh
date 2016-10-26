//
//  HWHd.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

@interface HWHd : HWItem

@property NSNumber *boot;
@property NSNumber *controller;
@property NSNumber *bus;
@property NSString *file;
@property NSString *size;

@end
