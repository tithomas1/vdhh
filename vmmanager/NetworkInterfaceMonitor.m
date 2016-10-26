//
//  NetworkInterfaceMonitor.m
//  vmx
//
//  Created by Boris Remizov on 07/10/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "NetworkInterfaceMonitor.h"
#import <SystemConfiguration/SystemConfiguration.h>

@interface NetworkInterfaceMonitor()

@property (nonatomic, strong) NSArray* interfaces;

@end

static void StoreCallBack(SCDynamicStoreRef store, CFArrayRef changedKeys, void* info) {
    assert([NSThread isMainThread]);
    NetworkInterfaceMonitor* me = (__bridge NetworkInterfaceMonitor*)info;
    me.interfaces = CFBridgingRelease(SCNetworkInterfaceCopyAll());
}

@implementation NetworkInterfaceMonitor
{
    SCDynamicStoreRef _store;
    CFRunLoopSourceRef _source;
}

- (instancetype)init {
    self = [super init];
    if (!self)
        return nil;

    SCDynamicStoreContext context = {
        0,
        (__bridge void*)self,
        CFRetain, CFRelease, CFCopyDescription
    };
    _store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("NetworkInterfaceModitor"), StoreCallBack, &context);

    NSArray *patterns = @[@"State:/Network/Interface"];

    SCDynamicStoreSetNotificationKeys(_store, NULL, (__bridge CFArrayRef)patterns);

    _source = SCDynamicStoreCreateRunLoopSource(NULL, _store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), _source, kCFRunLoopCommonModes);

    _interfaces = (__bridge NSArray*)SCNetworkInterfaceCopyAll();

    return self;
}

- (void)dealloc {
    CFRunLoopSourceInvalidate(_source);
    CFRelease(_source);
    CFRelease(_store);
}

@end
