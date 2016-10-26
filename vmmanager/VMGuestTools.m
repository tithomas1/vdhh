//
//  VMGuestTools.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/27/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import "VMGuestTools.h"

@implementation VMGuestTools

-(NSNumber*) file_sharing {
    if (_file_sharing == nil)
        return @TRUE;
    return _file_sharing;
}

-(NSNumber*) copy_paste {
    if (_copy_paste == nil)
        return @TRUE;
    return _copy_paste;
}


@end
