//
//  ScopedValueObserver.h
//  vmx
//
//  Created by Boris Remizov on 31/08/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ScopedValueObserver : NSObject

- (id)observe:(id)object forKeyPath:(NSString*)keyPath change:(void(^)(id newValue))change;
- (void)removeObserver:(id)observer;

@end
