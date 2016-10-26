//
//  HWCpu.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "HWCpu.h"

@implementation HWCpu
/*@synthesize cores = _cores;
@synthesize sockets = _sockets;
@synthesize threads = _threads;*/
-(NSNumber *)cores {
    if (_cores == nil)
        return @1;
    return _cores;
}
-(NSNumber *)sockets {
    if (_sockets == nil)
        return @1;
    return _sockets;
}
-(NSNumber *)threads {
    if (_threads == nil)
        return @1;
    return _threads;
}
@end
