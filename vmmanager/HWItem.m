//
//  HWItem.m
//  vmx
//
//  Created by Boris Remizov on 24/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "HWItem.h"

@interface HWItem()

/// \brief Limit of connection capability of the the item.
///        It's assumed all items are organized into tree hierarchy, when certain
///        item could have some connected (clients) items, also the item could be
///        connected to provider by itself. Root item is not connected to any providers.
@property (nonatomic, strong, readonly) NSNumber* maxClients;

@end

@implementation HWItem

static NSUInteger sId = 0;

- (instancetype)initFromDictionary:(NSDictionary *)dict {
    self = [super initFromDictionary:dict];
    if (!self)
        return nil;

    // import mode
    if (nil == self.id) {
        self.id = @(sId++);
    }
//    NSAssert(self.id, @"Serialized HWItem instance should have id assigned");

    if (nil == self.id) {
        // bad bad bad - serialized item should have identifier assigned
        self.id = @0;
    }
    return self;
}

- (NSUInteger)hash {
    return [self.id hash];
}

- (BOOL)isEqual:(id)object {
    // equal could be only objects of same type
    if (![[object className] isEqual: [self className]])
        return FALSE;

    NSAssert(self.id && [(HWItem*)object id], @"All HWItem objects must have non-empty identifiers");
    return [self.id isEqual: [(HWItem*)object id]];
}

- (NSUInteger)clientsLimit {
    return self.maxClients ? [self.maxClients unsignedIntegerValue] : NSUIntegerMax;
}

@end
