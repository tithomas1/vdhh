//
//  ScopedValueObserver.m
//  vmx
//
//  Created by Boris Remizov on 31/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "ScopedValueObserver.h"

@interface ScopedValueObserver()

@property (nonatomic, strong) NSMutableDictionary* contexts;

@end

@implementation ScopedValueObserver
{
    uint8* _lastContext;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;
    self.contexts = [NSMutableDictionary dictionary];

    return self;
}

- (void)dealloc
{
    // remove observers
    for (NSValue* context in self.contexts) {
        NSDictionary* info = self.contexts[context];
        [[info[@"object"] nonretainedObjectValue] removeObserver:self
            forKeyPath:info[@"keyPath"] context:[context pointerValue]];
    }
}

- (id)observe:(id)object forKeyPath:(NSString*)keyPath change:(void(^)(id newValue))change
{
    void* newContext = ++_lastContext;
    NSDictionary* info = @{
        @"object" : [NSValue valueWithNonretainedObject:object],
        @"keyPath" : [keyPath copy],
        @"change" : [change copy]
    };

    self.contexts[[NSValue valueWithPointer:newContext]] = info;
    [object addObserver:self forKeyPath:keyPath options:NSKeyValueObservingOptionNew context:newContext];

    return [NSValue valueWithPointer:newContext];
}

- (void)removeObserver:(id)context
{
    NSDictionary* info = self.contexts[context];
    [[info[@"object"] nonretainedObjectValue] removeObserver:self
        forKeyPath:info[@"keyPath"] context:[context pointerValue]];
    [self.contexts removeObjectForKey:context];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *,id> *)change
                       context:(void *)context
{
    NSDictionary* info = self.contexts[[NSValue valueWithPointer:context]];
    void(^block)(id newValue) = info[@"change"];
    block(change[NSKeyValueChangeNewKey]);
}

@end
