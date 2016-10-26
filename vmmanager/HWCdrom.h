//
//  HWCdrom.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

@interface HWCdrom : HWItem

@property NSNumber *controller;
@property NSNumber *bus;
@property NSString *file;
@property NSString *type;
@property (nonatomic) BOOL media_in;

@end

