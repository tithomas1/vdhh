//
//  NetworkInterfaceMonitor.h
//  vmx
//
//  Created by Boris Remizov on 07/10/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NetworkInterfaceMonitor : NSObject

@property (nonatomic, readonly, strong) NSArray* interfaces;

@end
