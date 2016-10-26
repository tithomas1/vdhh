//
//  VMAdvanced.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/27/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "VMAdvanced.h"

@implementation VMPortForwarding
-(NSString *)toQemuPortForwardingCmd {
    return [NSString stringWithFormat:@"%@:%@:%@-%@:%@",
            self.protocol, self.host_ip, self.host_port,
            self.guest_ip ? self.guest_ip : @"", self.guest_port];
}

-(NSString *)toVirtualBoxPortForwardingCmd {
    return [NSString stringWithFormat:@"%@,%@,%@,%@,%@,%@",
            self.name, self.protocol, self.host_ip, self.host_port,
            self.guest_ip ? self.guest_ip : @"", self.guest_port];
}
@end

@implementation VMAdvanced
+(void)_T_port_forwardings_T_VMPortForwarding{}
-(NSArray<VMPortForwarding *> *) port_forwarding_as_list {
    NSMutableArray<VMPortForwarding *> *rv = [NSMutableArray array];
    for (NSString *label in self.port_forwardings) {
        VMPortForwarding *pf = self.port_forwardings[label];
        pf.name = label;
        [rv addObject:pf];
    }
    return rv;
}
@end
