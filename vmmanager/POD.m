//
//  POD.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/23/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "POD.h"
#import "objc/runtime.h"

@implementation POD

-(NSString *)description {
    return [[[self class] description] stringByAppendingString:[[self toDictionary] description]];
}

+ (NSArray*)propertiesOfClass:(Class)cls {

    NSMutableArray* allProperties = [NSMutableArray array];

    unsigned int outCount, i;
    objc_property_t *properties = class_copyPropertyList(cls, &outCount);
    for(i = 0; i < outCount; ++i) {
        const objc_property_t property = properties[i];
        const char *propName = property_getName(property);
        if (propName)
            [allProperties addObject:[NSString stringWithUTF8String:propName]];
    }
    free(properties);
    return allProperties;
}

- (NSArray*)allKeys
{
    NSMutableSet* properties = [NSMutableSet set];
    Class cls = [self class];
    while(cls != [POD class]) {
        [properties addObjectsFromArray:[POD propertiesOfClass:cls]];
        cls = class_getSuperclass(cls);
    }
    return [properties allObjects];
}

- (NSString*)attributesOfKey:(NSString*)key
{
    Class cls = [self class];
    while(cls != [POD class]) {
        objc_property_t prop = class_getProperty(cls, [key UTF8String]);
        if (prop)
            return [NSString stringWithUTF8String:property_getAttributes(prop)];
        cls = class_getSuperclass(cls);
    }
    return nil;
}

-(NSDictionary *)toDictionary {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];

    for (NSString* key in self.allKeys) {
        id value = [self valueForKey: key];

        // skip empty fields
        if (nil == value)
            continue;

        d[key] = [POD clone:value];
    }
    return d;
}

+(NSArray<NSString *> *)typeMethodToType:(NSString *)name {
    static NSRegularExpression *regex;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        regex = [NSRegularExpression
                 regularExpressionWithPattern:@"_T_(.*)_T_(.*)"
                 options:0
                 error:nil];
    });
    NSTextCheckingResult *r = [regex firstMatchInString:name options:0 range:NSMakeRange(0, [name length])];
    if (!r)
        return nil;
    return @[[name substringWithRange:[r rangeAtIndex:1]],
             [name substringWithRange:[r rangeAtIndex:2]]];
}

+(NSDictionary<NSString *, NSString *> *)typeMethods {
    unsigned int methodCount = 0;
    const char *cls = [NSStringFromClass([self class]) cStringUsingEncoding:NSUTF8StringEncoding];
    Method *methods = class_copyMethodList(objc_getMetaClass(cls), &methodCount);
    NSMutableDictionary *rv = [NSMutableDictionary dictionary];

    for (unsigned int i = 0; i < methodCount; i++) {
        Method method = methods[i];
        NSString *name = [NSString stringWithUTF8String:sel_getName(method_getName(method))];
        NSArray<NSString *> *methodAndType = [self typeMethodToType:name];
        if (methodAndType)
            rv[methodAndType[0]] = methodAndType[1];
    }

    free(methods);
    return rv;
}

-(instancetype)initFromDictionary:(NSDictionary *)dict {
    unsigned int outCount, i;
    self = [super init];
    if (!self)
        return nil;

    objc_property_t *properties = class_copyPropertyList([self class], &outCount);
    NSDictionary<NSString *, NSString *> *types = [[self class] typeMethods];

    for (NSString* key in self.allKeys) {
        NSString* propAttr = [self attributesOfKey:key];
        if (!propAttr)
            continue;

        NSString *type = nil;
        if ([propAttr rangeOfString:@"T@\""].location == 0) {
            NSRange range = [propAttr rangeOfString:@"\"" options:0 range:NSMakeRange(3, [propAttr length] - 3)];
            NSAssert(NSNotFound != range.location, @"Inconsistent attribute string %@", propAttr);
            type = [propAttr substringWithRange:NSMakeRange(3, range.location - 3)];
        } else if ([propAttr rangeOfString:@"Tc,"].location == 0) {
            type = @"BOOL";
        } else {
            [NSException raise:@"illegal property in POD"
                        format:@"property %@ has unrecognize type, propAttr %s", key, propAttr];
        }
        
        if ([type isEqual:@"BOOL"]) {
            if (dict[key])
                [self setValue:dict[key] forKey:key];
            else
                [self setValue:[NSNumber numberWithBool:false] forKey:key];
            continue;
        }
        
        NSObject *value;
        Class typeClass = NSClassFromString(type);
        if ([type isEqualToString:@"NSArray"]) {
            NSString *inner = types[key];
            if (!inner)
                @throw [NSException exceptionWithName:@"IllegalState"
                                               reason:[NSString stringWithFormat:@"cannot find inner type of type %@", key]
                                             userInfo:nil];
            NSMutableArray *ar = [NSMutableArray arrayWithCapacity:[dict[key] count]];
            Class cls = NSClassFromString(inner);
            NSScriptClassDescription *desc = [NSScriptClassDescription
                                              classDescriptionForClass:[self class]];
            for (int i=0;i < [dict[key] count]; i++) {
                id obj = dict[key][i];
                if ([cls isSubclassOfClass:[POD class]]) {
                    POD *pod = [cls alloc];
                    pod.objectSpecifier = [[NSIndexSpecifier alloc]
                                           initWithContainerClassDescription:desc
                                           containerSpecifier:self.objectSpecifier
                                           key:key index:i];
                    [ar addObject:[pod initFromDictionary:obj]];
                } else
                    [ar addObject:obj];
            }
            value = ar;
        } else if ([typeClass isSubclassOfClass: [NSDictionary class]]) {
            NSString *inner = types[key];
            if (!inner)
                @throw [NSException exceptionWithName:@"IllegalState"
                                               reason:[NSString stringWithFormat:@"cannot find inner type of type %@", key]
                                             userInfo:nil];
            NSMutableDictionary<NSString *, id> *datadict = [NSMutableDictionary dictionary];
            Class cls = NSClassFromString(inner);
            NSScriptClassDescription *desc = [NSScriptClassDescription
                                              classDescriptionForClass:[self class]];
            for (NSString *obj_key in dict[key]) {
                id obj = dict[key][obj_key];
                if ([cls isSubclassOfClass:[POD class]]) {
                    POD *pod = [cls alloc];
                    pod.objectSpecifier = [[NSNameSpecifier alloc]
                                           initWithContainerClassDescription:desc
                                           containerSpecifier:self.objectSpecifier
                                           key:key name:obj_key];
                    datadict[obj_key] = [pod initFromDictionary:obj];
                } else
                    datadict[obj_key] = obj;
            }
            value = datadict;
        } else if ([typeClass isSubclassOfClass: [POD class]]) {
            value = [typeClass alloc];
            POD *pod = (POD *)value;
            pod.objectSpecifier = [[NSPropertySpecifier alloc]
                                   initWithContainerSpecifier:self.objectSpecifier key:key];

            value = [pod initFromDictionary:dict[key]];
        } else
            value = dict[key];

        [self setValue:value forKey:key];
    }
    free(properties);
    return self;
}

+(instancetype)fromDictionary:(NSDictionary *)dict {
    POD *pod = [[self alloc] initFromDictionary:dict];
    return pod;
}

-(NSScriptObjectSpecifier * _Nonnull)objectSpecifier {
    //NSLog(@"whoa: %@", _objectSpecifier);
    return _objectSpecifier;
}

+(id)clone:(id)x {
    if ([x isKindOfClass:[POD class]])
        return [x toDictionary];
    if ([x isKindOfClass:[NSArray class]])
        return [POD clonePODArray:x];
    if ([x isKindOfClass:[NSDictionary class]])
        return [POD clonePODDictionary:x];
    return x;
}

+(NSDictionary *)clonePODDictionary:(NSDictionary *)dict {
    NSMutableDictionary *d = [NSMutableDictionary dictionaryWithDictionary:dict];
    for (id key in dict) {
        id val = d[key];
        if ([val isKindOfClass:[POD class]]) {
            d[key] = [val toDictionary];
        }
    }
    return d;
}

+(NSArray *)clonePODArray:(NSArray *)ar {
    NSMutableArray *a = [NSMutableArray arrayWithArray:ar];
    for (int i=0; i<[a count]; i++) {
        id val = a[i];
        if ([val isKindOfClass:[POD class]]) {
            a[i] = [val toDictionary];
        }
    }
    return a;
}

- (id)copyWithZone:(nullable NSZone *)zone {
    NSObject *clone = [[[self class] alloc] init];
    [clone setValuesForKeysWithDictionary:[self toDictionary]];
    return clone;
}

- (nullable id)objectForKeyedSubscript:(NSString*)key {
    return [self valueForKey:key];
}

- (void)setObject:(nullable id)object forKeyedSubscript:(nonnull NSString*)key {
    [self setValue:object forKey:key];
}

@end
