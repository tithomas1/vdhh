/*
 * Copyright (C) 2016 Veertu Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#import <Foundation/Foundation.h>
#import <VMManager/VMLibrary.h>
#import "vmlibrary_ops.h"

bool get_launch_param_string(char *vm_path, bool restore, char *buf, int buf_len)
{
    NSString *vmname = [NSString stringWithUTF8String: vm_path];
    VM *vm = [[VMLibrary sharedVMLibrary] readVmProperties: vmname];
    if (!vm)
        return false;
    NSArray *p = [[VMLibrary sharedVMLibrary] createLaunchVmParam: vmname  withOptions: vm restore: restore];

    buf[0] = 0;
    for (NSString *param in p) {
        const char *str = [[NSString stringWithFormat: @"\"%@\"", param] UTF8String];
        strncat(buf,str, buf_len);
        buf_len -= [param length];
        strncat(buf, " ", buf_len);
        buf_len--;
        if (buf_len <= 0)
            break;
    }
    return true;
}

#define VMSETTINGS_CACHE_TIME_SEC   5

bool get_addons_settings(const char *vm_name, struct VMAddOnsSettings *settings)
{
    static char last_vm_name[128] = {0};
    static VMAddOnsSettings last_vm_settings;
    static time_t last_updated;
    
    memset(settings, 0, sizeof(*settings));

    if (!strcmp(last_vm_name, vm_name) && (time(NULL) - last_updated) < VMSETTINGS_CACHE_TIME_SEC) {
        memcpy(settings, &last_vm_settings, sizeof(*settings));
        return true;
    }

    NSString *vmname = [NSString stringWithUTF8String: vm_name];
    VM *vm = [[VMLibrary sharedVMLibrary]  readVmProperties: vmname];
    if (!vm)
        return false;
    
    if (vm.advanced) {

        if (vm.advanced.remap_cmd)
            settings->remap_cmd = true;
        else
            settings->remap_cmd = false;

        if (vm.advanced.hdpi)
            settings->hdpi = true;
        else
            settings->hdpi = false;


        settings->enable_fs = true;
        if (vm.advanced.guest_tools) {
            if (![vm.advanced.guest_tools.file_sharing boolValue])
                settings->enable_fs = false;
         
            if (settings->enable_fs) {
                NSString *folder = RealDownloadsDirectory();
                if (vm.advanced.guest_tools.fs_folder) {
                    folder = vm.advanced.guest_tools.fs_folder;
                }
                strcpy(settings->fs_folder, [folder UTF8String]);
            }

            settings->enable_copy_paste = vm.advanced.guest_tools.copy_paste;
        }
    }

    strcpy(last_vm_name, vm_name);
    memcpy(&last_vm_settings, settings, sizeof(*settings));
    last_updated = time(NULL);
    return true;
}

uint64_t get_fs_free_space(char *folder)
{
    NSString *f = [NSString stringWithUTF8String: folder];
    NSError *err;
    NSDictionary* fileAttributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath:f error:&err];
    if (err != nil)
        NSLog(@"Error getting fs_free_space %@", err);
    uint64_t size = [[fileAttributes objectForKey:NSFileSystemFreeSize] longLongValue];
    return size;
}

uint64_t get_fs_size(char *folder)
{
    NSString *f = [NSString stringWithUTF8String: folder];
    NSError *err;
    NSDictionary* fileAttributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath:f error:&err];
    if (err != nil)
        NSLog(@"Error getting fs_free_space %@", err);
    uint64_t size = [[fileAttributes objectForKey:NSFileSystemSize] longLongValue];
    return size;
}

void get_platform_uuid(char *buf, int bufSize)
{
    // Store the data
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    NSString *uuid = [defaults objectForKey: @"uuid"];
    if (uuid) {
        strncpy(buf, [uuid UTF8String], bufSize);
        return;
    }

    uuid = [[NSUUID UUID] UUIDString];
    [defaults setObject:uuid forKey:@"uuid"];
    [defaults synchronize];

    strncpy(buf, [uuid UTF8String], bufSize);
}
