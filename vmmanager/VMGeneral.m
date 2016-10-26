//
//  VMGeneral.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "VMGeneral.h"

@implementation VMGeneral
- (void)setValuesForKeysWithDictionary:(NSDictionary<NSString *,
                                        id> *)keyedValues
{
    /* we need to stray from usual implementation since description is a reserved obj-C equivalent
     * for Java's toString()
     */
    if (keyedValues[@"description"] == nil) {
        NSMutableDictionary *d = [NSMutableDictionary dictionaryWithDictionary:keyedValues];
        id x = d[@"description"];
        d[@"_description"] = x;
        [d removeObjectForKey:@"description"];
        [super setValuesForKeysWithDictionary:d];
    } else
        [super setValuesForKeysWithDictionary:keyedValues];
}
@end