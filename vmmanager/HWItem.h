//
//  HWItem.h
//  vmx
//
//  Created by Boris Remizov on 24/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/POD.h>

/// \brief Base class for all objects, representing hardware components.
///        Objects of this type are comparable by isEqual: and hash methods
@interface HWItem : POD

/// \brief Unique identifier of the item within its scope (not globally).
@property (nonatomic, strong) NSNumber *id;

/// \brief Identifier of provider item.
///        nil if no providers, in case of detached or root item.
@property (nonatomic, strong) NSNumber* provider;

- (NSUInteger)clientsLimit;

@end
