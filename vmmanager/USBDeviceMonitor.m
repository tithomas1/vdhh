//
//  USBDeviceMonitor.m
//  vmx
//
//  Created by Boris Remizov on 31/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "USBDeviceMonitor.h"
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/IOUSBLib.h>

@interface USBDeviceMonitor()

@property (nonatomic, strong) NSArray* connectedDevices;

@end

static void USBDevicePublishCallback(void* refcon, io_iterator_t it)
{
    USBDeviceMonitor* me = (__bridge USBDeviceMonitor*)refcon;
    NSMutableArray* usbDevices = [NSMutableArray arrayWithArray:me.connectedDevices];

    io_service_t dev = 0;
    while((dev = IOIteratorNext(it))) {

        CFMutableDictionaryRef props = NULL;
        IORegistryEntryCreateCFProperties(dev, &props, kCFAllocatorDefault, 0);

        NSMutableDictionary* devInfo = [NSMutableDictionary dictionaryWithDictionary:(__bridge_transfer NSDictionary*)props];
        NSUInteger idx = [usbDevices indexOfObjectPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop){
            return [devInfo[@"locationID"] isEqual:obj[@"locationID"]];
        }];

        if (NSNotFound == idx)
            [usbDevices addObject:devInfo];

        IOObjectRelease(dev);
    }

    // update contents
    me.connectedDevices = usbDevices;
}

static void USBDeviceRemoveCallback(void* refcon, io_iterator_t it)
{
    USBDeviceMonitor* me = (__bridge USBDeviceMonitor*)refcon;
    NSMutableArray* usbDevices = [NSMutableArray arrayWithArray:me.connectedDevices];

    io_service_t dev = 0;
    while((dev = IOIteratorNext(it))) {
        CFMutableDictionaryRef props = NULL;
        IORegistryEntryCreateCFProperties(dev, &props, kCFAllocatorDefault, 0);

        NSDictionary* devInfo = (__bridge_transfer NSDictionary*)props;
        NSUInteger idx = [usbDevices indexOfObjectPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop){
            return [devInfo[@"locationID"] isEqual:obj[@"locationID"]];
        }];

        if (NSNotFound != idx)
            [usbDevices removeObjectAtIndex:idx];

        IOObjectRelease(dev);
    }

    // update contents
    me.connectedDevices = usbDevices;
}

@implementation USBDeviceMonitor
{
    IONotificationPortRef _port;
    io_iterator_t _addit;
    io_iterator_t _removeit;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return self;

    _port = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource (_port), kCFRunLoopCommonModes);

    // subscribe on changes in scope of IOUSBDevice/Interface objects
    kern_return_t res = IOServiceAddMatchingNotification(_port, kIOPublishNotification,
                                                         IOServiceMatching(kIOUSBDeviceClassName),
                                                         USBDevicePublishCallback, (__bridge void*)self, &_addit);
    USBDevicePublishCallback((__bridge void*)self, _addit);


    res = IOServiceAddMatchingNotification(_port, kIOTerminatedNotification,
                                           IOServiceMatching(kIOUSBDeviceClassName),
                                           USBDeviceRemoveCallback, (__bridge void*)self, &_removeit);
    USBDeviceRemoveCallback((__bridge void*)self, _removeit);

    return self;
}

- (void)dealloc
{
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource (_port), kCFRunLoopDefaultMode);
    IONotificationPortDestroy(_port);

    IOObjectRelease(_addit);
    IOObjectRelease(_removeit);
}

- (instancetype)copyWithZone:(NSZone*)unused
{
    return self;
}

@end
