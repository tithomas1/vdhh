//
//  HWNic.h
//  Veertu VMX
//
//  Created by VeertuLabs on 2/22/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/HWItem.h>

@interface HWNic : HWItem

@property NSString *connection;
@property NSString *pci_addr;
@property NSString *pci_bus;
@property NSString *mac;
@property NSString *model;
@property NSString *family;
@property NSString *bridge;

@end
