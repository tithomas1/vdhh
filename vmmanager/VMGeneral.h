//
//  VMGeneral.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "POD.h"

@interface VMGeneral : POD
@property NSString *_description;
@property NSString *os;
@property NSString *os_family;
@property NSString *boot_device;
- (void)setValuesForKeysWithDictionary:(NSDictionary<NSString *,
                                        id> *)keyedValues;
@end