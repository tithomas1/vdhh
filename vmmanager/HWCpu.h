//
//  HWCpu.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

@interface HWCpu : HWItem

@property (nonatomic) NSNumber *cores;
@property (nonatomic) NSNumber *sockets;
@property (nonatomic) NSNumber *threads;

@end