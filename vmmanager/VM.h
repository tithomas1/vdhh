//
//  VM.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/20/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/POD.h>
#import <VMManager/VMGeneral.h>
#import <VMManager/VMAdvanced.h>
#import <VMManager/VMHw.h>
#import <Foundation/Foundation.h>

@class VMLibrary;

NSArray *removeId(NSArray *a, NSNumber *id);
NSArray *replaceOrAdd(NSArray *a, NSNumber *old_id, NSObject *obj);

FOUNDATION_EXPORT NSString *const VMLaunchNotification;
FOUNDATION_EXPORT NSString *const VMUpdateNotification;

@interface VM : POD
@property NSString *display_name;
+(instancetype)withVMLibrary:(VMLibrary *)vmlib;
@property NSString *name;
@property VMGeneral *general;
@property VMHw *hw;
@property NSString *uuid;
@property NSString *version;
@property VMAdvanced *advanced;
- (NSString *) status;
@end
