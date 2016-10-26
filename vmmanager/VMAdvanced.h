//
//  VMAdvanced.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/27/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "VMGuestTools.h"

@interface VMPortForwarding : POD
@property (nonatomic) NSString *name;
@property (nonatomic) NSString *protocol;
@property (nonatomic) NSString *host_ip;
@property (nonatomic) NSNumber *host_port;
@property (nonatomic) NSString *guest_ip;
@property (nonatomic) NSNumber *guest_port;
-(NSString *)toQemuPortForwardingCmd;
-(NSString *)toVirtualBoxPortForwardingCmd;
@end

@interface VMAdvanced : POD

@property (nonatomic) BOOL headless;
@property (nonatomic) BOOL snapshot;
@property (nonatomic) BOOL hdpi;
@property (nonatomic) BOOL remap_cmd;
@property (nonatomic) NSMutableDictionary<NSString *, VMPortForwarding *> *port_forwardings;
-(NSArray<VMPortForwarding *> *) port_forwarding_as_list;
@property (nonatomic) VMGuestTools *guest_tools;

@end

