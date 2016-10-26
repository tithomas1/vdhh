//
//  HWUsb.m
//  vmx
//
//  Created by Boris Remizov on 23/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "HWUsb.h"

@implementation HWUsb

- (BOOL)isEqual:(id)object {
    if ([object isKindOfClass:[NSDictionary class]]) {
        // epty objects are not equal
        if (!self.vendorId && !self.productId && !self.locationId) {
//            NSAssert(FALSE, @"Empty USB device detected %@", self);
            return FALSE;
        }

        // compare object with dictionary (the IOKit properties) semantically
        if (self.locationId && ![self.locationId isEqual:object[@"locationID"]])
            return FALSE;
        if (self.vendorId && ![self.vendorId isEqual:object[@"idVendor"]])
            return FALSE;
        if (self.productId && ![self.productId isEqual:object[@"idProduct"]])
            return FALSE;

        return TRUE;
    } else {
        return [super isEqual:object];
    }
}

@end
