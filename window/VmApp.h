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

#import "VmView.h"
#import <VMManager/VMManager.h>
#import <Cocoa/Cocoa.h>

@interface VmAppController : NSObject <NSApplicationDelegate>

@property (nonatomic, readonly) VM* vm;
@property (nonatomic, readonly) VmView *vmView;

@property NSWindow *normalWindow;
@property (strong) NSWindowController *rootController;

- (instancetype)initWithVM:(VM*)vm;

- (void)startEmulationWithArgc:(int)argc argv:(char**)argv;

@end

@interface VmApp : NSApplication

@end
