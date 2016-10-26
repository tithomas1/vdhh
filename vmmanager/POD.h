//
//  POD.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/23/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface POD : NSObject<NSCopying>

@property (readwrite, nonatomic) NSScriptObjectSpecifier * _Nonnull objectSpecifier;

+(nonnull instancetype)fromDictionary:(nonnull NSDictionary *)dict;
+(nonnull instancetype)make UNAVAILABLE_ATTRIBUTE;

-(nonnull instancetype)initFromDictionary:(nonnull NSDictionary *)dict;

-(NSScriptObjectSpecifier * _Nonnull)objectSpecifier;
-(nonnull NSString *)description;
-(nonnull NSDictionary *)toDictionary;

-(nonnull instancetype)copyWithZone:(nullable NSZone *)zone;

- (nullable id)objectForKeyedSubscript:(nonnull NSString*)key;
- (void)setObject:(nullable id)object forKeyedSubscript:(nonnull NSString*)key;

@end
