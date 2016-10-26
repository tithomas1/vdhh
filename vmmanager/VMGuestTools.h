//
//  VMGuestTools.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/27/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "POD.h"

@interface VMGuestTools : POD

@property (nonatomic) NSNumber *file_sharing;
@property (nonatomic) NSNumber *copy_paste;
@property (nonatomic) NSString *fs_folder;

@end
