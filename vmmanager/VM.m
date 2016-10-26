//
//  VM.m
//  Veertu VMX
//
//  Created by VeertuLabs on 2/20/16.
//  Copyright © 2016 Veertu Labs Ltd. All rights reserved.
//

#import <VMManager/VMLibrary.h>
#import "VM.h"


NSArray *removeId(NSArray *a, NSNumber *id) {
    NSMutableArray *r = [NSMutableArray arrayWithCapacity:[a count]];
    for (NSObject *o in a) {
        if ([[o valueForKey:@"id"] isEqual: id])
            continue;
        [r addObject:o];
    }
    return r;
}

NSArray *replaceOrAdd(NSArray *a, NSNumber *old_id, NSObject *obj) {
    NSMutableArray *r = [NSMutableArray arrayWithCapacity:[a count]];
    bool added = false;
    for (NSObject *o in a) {
        if ([[o valueForKey:@"id"] isEqual: old_id]) {
            [r addObject:obj];
            added = true;
        } else
            [r addObject:o];
    }
    if (!added)
        [r addObject:obj];
    return r;
}

@implementation VM  {
    VMLibrary *vmlib;
}

- (id)handleStart:(NSScriptCommand *)scriptCommand {
    return @([[VMLibrary sharedVMLibrary] startVm:self.name]);
}
- (id)handleShutDown:(NSScriptCommand *)scriptCommand {
    NSNumber *r = [NSNumber numberWithBool:
                   [[VMLibrary sharedVMLibrary] shutdownVm:self.name]];
    return r;
}
- (id)handleForceShutDown:(NSScriptCommand *)scriptCommand {
    [[VMLibrary sharedVMLibrary] shutoffVm:self.name];
    return [NSNumber numberWithBool:YES];
}
- (id)handleSuspend:(NSScriptCommand *)scriptCommand {
    NSNumber *r = [NSNumber numberWithBool:
                   [[VMLibrary sharedVMLibrary] suspendVm:self.name]];
    return r;
}
- (id)handleRestart:(NSScriptCommand *)scriptCommand {
    return @([[VMLibrary sharedVMLibrary] restartVm:self.name] != -1);
}

- (id)handleResume:(NSScriptCommand *)scriptCommand {
    return @([[VMLibrary sharedVMLibrary] startVm:self.name] != -1);
}

- (id)handleDelete:(NSScriptCommand *)scriptCommand {
    NSNumber *r = [NSNumber numberWithBool:
                   [[VMLibrary sharedVMLibrary] deleteVm:self.name]];
    [[NSNotificationCenter defaultCenter] postNotificationName:VMUpdateNotification object:nil];
    return r;
}
- (id)handleSetHeadless:(NSScriptCommand *)scriptCommand {
    [[VMLibrary sharedVMLibrary] setHeadlessMode: self.name value: [scriptCommand.arguments[@"value"] boolValue]];
    return [NSNumber numberWithBool:TRUE];
}

- (NSString *) status {
    VMState state = [vmlib getVmState: self.name];
    switch (state) {
        case VMStateStopped:
            return @"stopped";
        case VMStateSuspended:
            return @"paused";
        case VMStateStarting:
            return @"starting";
        case VMStateRunning:
            return @"running";
        case VMStateSuspending:
            return @"suspending";
        case VMStateShuttingDown:
            return @"shutting down";
        default:
            break;
    }
    return @"undefined/power off";
}

- (NSString *) ip {
    return [vmlib getIpAddress: self.name];
}

+(VM *)withVMLibrary:(VMLibrary *)vmlib {
    VM *vm = [[VM alloc] init];
    vm->vmlib = vmlib;
    return vm;
}

-(void) setDisplayName:(NSScriptCommand *)scriptCommand{
    [[VMLibrary sharedVMLibrary] renameVM:self.name to: scriptCommand.arguments[@"value"]];
}

-(id) setCpuCount:(NSScriptCommand *) scriptCommand{
    NSNumber *cores = scriptCommand.arguments[@"value"];
    [[VMLibrary sharedVMLibrary] setVmCpuInfo:self cores: cores threads:nil sockets: nil];
    return [NSNumber numberWithBool:TRUE];
}

-(id) setMemory:(NSScriptCommand *) scriptCommand{
    NSString *memory = scriptCommand.arguments[@"value"];
    memory = [memory uppercaseString];
    NSString *regex = @"(\\d{1,4}MB|\\dGB)";
    NSPredicate *memoryStringTest = [NSPredicate predicateWithFormat:@"SELF MATCHES %@", regex];
    if([memoryStringTest evaluateWithObject:memory]){
        [[VMLibrary sharedVMLibrary] setVmMemory:self memory: memory];
        return [NSNumber numberWithBool:TRUE];
    }
    // in case regex didn't pass
    return [NSNumber numberWithBool:FALSE];
    
}

-(id) setNicConnectionType:(NSScriptCommand *) scriptCommand{
    NSArray<NSString*> *conn_types = [self getNicConnectionTypes];
    NSNumber *nic_id = scriptCommand.arguments[@"nic_id"];
    NSString *nic_type = scriptCommand.arguments[@"value"];
    if(![conn_types containsObject:nic_type])
        return[NSNumber numberWithBool:FALSE];
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: self.name];
    VMHw *hw = vm.hw;
    NSArray<HWNic *> *nics = hw.nic;
    bool changed = false;
    
    NSMutableArray<HWNic *> *new_nics = [NSMutableArray array];
    for (HWNic *nic in nics) {
        if ([nic.id intValue] == [nic_id intValue] && ![nic.connection isEqualToString:nic_type]) {
            changed = true;
            nic.connection = nic_type;
        }
        [new_nics addObject: nic];
    }
    if (changed)
        [[VMLibrary sharedVMLibrary] setNics: self.name nics: new_nics];
    return [NSNumber numberWithBool:TRUE];
    
}

-(id) addNic:(NSScriptCommand *) scriptCommand{
    NSArray<NSString*> *conn_types = [self getNicConnectionTypes];
    NSArray<NSString*> *model_types = [[VMLibrary sharedVMLibrary] getNicModels];
    NSString *nic_type = scriptCommand.arguments[@"conn_type"];
    NSString *model_type = scriptCommand.arguments[@"model"];
    if(![conn_types containsObject:nic_type] || ![model_types containsObject:model_type])
        return[NSNumber numberWithBool:FALSE];
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: self.name];
    VMHw *hw = vm.hw;
    NSArray<HWNic *> *nics = hw.nic;
    HWNic *new_nic = [[VMLibrary sharedVMLibrary] createNewNic: self.name];
    new_nic.connection = nic_type;
    new_nic.model = model_type;
    NSMutableArray<HWNic *> *new_nics = [NSMutableArray array];
    for (HWNic *nic in nics) {
        [new_nics addObject: nic];
    }
    [new_nics addObject:new_nic];
    [[VMLibrary sharedVMLibrary] setNics: self.name nics: new_nics];
    return [NSNumber numberWithBool:TRUE];
    
    
}

-(id) removeNic:(NSScriptCommand *) scriptCommand{
    NSNumber *nic_index = scriptCommand.arguments[@"index"];
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: self.name];
    VMHw *hw = vm.hw;
    NSArray<HWNic *> *nics = hw.nic;
    NSMutableArray<HWNic *> *new_nics = [NSMutableArray array];
    BOOL removed = false;
    for (HWNic *nic in nics) {
        if([nic.id intValue] != [nic_index intValue])
        {
            [new_nics addObject: nic];
        }
        else
        {
            removed = true;
        }
    }
    if(!removed)
        return [NSNumber numberWithBool:FALSE];
    [[VMLibrary sharedVMLibrary] setNics: self.name nics: new_nics];
    return [NSNumber numberWithBool:TRUE];
}

-(NSArray<NSString*>*) getNicConnectionTypes{
    return @[
        @"bridged",
        @"shared",
        @"host",
        @"disconnected"
    ];
}

@end

/* moved here, since vmx compiles VM.m, and hence complains if doesn't
 * have those files at link time.
 */
NSString *const VMLaunchNotification = @"com.veertu.launchVm";
NSString *const VMUpdateNotification = @"com.veertu.updateVm";
