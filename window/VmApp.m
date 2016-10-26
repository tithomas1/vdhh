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

#import "VmApp.h"
#import "VmView.h"
#import "cocoa_util.h"
#import "VmxClipView.h"
#import "ui/input.h"
#import "sysemu.h"
#import "snapshot.h"
#import "qmp-commands.h"
#import "usb.h"
#import "qemu/config-file.h"
#import "qemu/option.h"
#import "monitor/qdev.h"
//#import "qom/object.h"
#import "VMManager/USBDeviceMonitor.h"
#import "VMManager/VMLibrary.h"
#import "VMManager/vmlibrary_ops.h"
#import "VMManager/ScopedValueObserver.h"

#import <IOKit/IOKitLib.h>
#import <IOKit/storage/IOMedia.h>
#import <DiskArbitration/DiskArbitration.h>

static QEMUBH *bh = NULL;

extern int gArgc;
extern char **gArgv;

@implementation VmApp

- (void)sendEvent:(NSEvent *)event
{
    if ([event type] == NSKeyUp && ([event modifierFlags] & NSCommandKeyMask))
        [[self keyWindow] sendEvent:event];
    else
        [super sendEvent:event];
}

@end


@interface VmAppController()

@property (nonatomic, strong) NSMenuItem* usbMenuItem;
@property (nonatomic, strong) NSMenuItem* cdromMenuItem;

@property (nonatomic, strong) VM* vm;
@property (nonatomic, strong) USBDeviceMonitor* usbMonitor;
@property (nonatomic, strong) ScopedValueObserver* observer;

@property (nonatomic, strong) VmView *vmView;

@property (nonatomic, copy) NSString *vm_name DEPRECATED_ATTRIBUTE;

@end

@implementation VmAppController

- (IBAction)showInFinder:(id)sender {
    HWCdrom* cd = [sender representedObject];
    if (![cd isKindOfClass:[HWCdrom class]])
        return;

    if (nil == cd.file)
        return;

    [[NSWorkspace sharedWorkspace] openFile:[cd.file stringByDeletingLastPathComponent]];
}

- (void)updateCdMenu:(id)unused
{
    NSMenu* menu = self.cdromMenuItem.submenu;

    [menu removeAllItems];

    if ([self.vm.hw.cdrom count] == 0) {
        NSMenuItem* name = [NSMenuItem new];
        name.title = NSLocalizedString(@"No Optical Drives Specified in VM Settings", "");
        name.enabled = false;
        [menu addItem:name];
        return;
    }

    // work only with cdrom.0 to simplify UI
    HWCdrom* cd = self.vm.hw.cdrom[0];
    if (cd.media_in && cd.file) {
        NSMenuItem* name = [NSMenuItem new];
        name.title = [cd.file lastPathComponent];
        [menu addItem:name];
        [menu addItem:[NSMenuItem separatorItem]];

        NSMenuItem* show = [NSMenuItem new];
        show.title = NSLocalizedString(@"Show in Finder", "");
        show.target = self;
        show.action = @selector(showInFinder:);
        show.representedObject = cd;
        [menu addItem:show];

        NSMenuItem* eject = [NSMenuItem new];
        eject.title = NSLocalizedString(@"Eject", "");
        eject.target = self;
        eject.action = @selector(performEjectCdrom:);
        eject.representedObject = cd;
        [menu addItem:eject];
    } else {
        NSMenuItem *item = [NSMenuItem new];
        item.title = NSLocalizedString(@"Attach .ISO...", "");
        item.target = self;
        item.action = @selector(performAttachCdrom:);
        [item setRepresentedObject: cd];
        [menu addItem: item];
    }
}

// check the USB device is already pass-throwed to guest
static DeviceState* obj_for_usbinfo(NSDictionary* devInfo) {
    NSString* devid = [NSString stringWithFormat:@"usb%u", [devInfo[@"locationID"] unsignedIntValue]];
    return qdev_find_recursive(sysbus_get_default(), [devid UTF8String]);
}

static void DiskUnmountCallback(DADiskRef disk, DADissenterRef __nullable dissenter, void * __nullable context ) {

    void(^completion)(NSError*) = CFBridgingRelease(context);

    // return status
    NSError* error = nil;
    if (dissenter && kDAReturnSuccess != DADissenterGetStatus(dissenter)) {
        NSString* errorDescriptiron = (__bridge NSString*)DADissenterGetStatusString(dissenter);
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:DADissenterGetStatus(dissenter) userInfo:@{
            NSLocalizedFailureReasonErrorKey: errorDescriptiron ? errorDescriptiron : NSLocalizedString(@"Disk Busy", "")
        }];
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        completion(error);
    });
}

- (void)unmountDisk:(UInt32)devLocation completion:(void(^)(NSError* error))completion {
    CFMutableDictionaryRef match = IOServiceMatching("IOMedia");
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    io_iterator_t medias = NULL;
    auto result = IOServiceGetMatchingServices(kIOMasterPortDefault, match, &medias);
    if (KERN_SUCCESS != result) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
        completion(error);
        return;
    }

    CFStringRef bsdName = NULL;
    io_service_t media = NULL;
    while(media = IOIteratorNext(medias)) {
        CFNumberRef locationID = IORegistryEntrySearchCFProperty(media, kIOServicePlane, CFSTR("locationID"),
            kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);

        if (!locationID) {
            IOObjectRelease(media);
            continue;
        }

        UInt32 location = 0;
        if (!CFNumberGetValue(locationID, kCFNumberSInt32Type, &location)) {
            CFRelease(locationID);
            IOObjectRelease(media);
            continue;
        }

        if (location == devLocation) {
            bsdName = IORegistryEntryCreateCFProperty(media, CFSTR("BSD Name"), kCFAllocatorDefault, 0);
            IOObjectRelease(media);
            break;
        }
        IOObjectRelease(media);
    }

    IOObjectRelease(medias);

    if (NULL == bsdName) {
        // no corresponding disks mounted
        completion(nil);
        return;
    }

    char name[32] = {0};
    CFStringGetCString(bsdName, name, sizeof(name), kCFStringEncodingMacRoman);
    CFRelease(bsdName);

    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (NULL == session) {
        NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:kDAReturnNoResources userInfo:nil];
        completion(error);
        return;
    }

    DASessionScheduleWithRunLoop(session, CFRunLoopGetMain(), kCFRunLoopCommonModes);

    DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, name);
    if (NULL == disk) {
        DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
        CFRelease(session);
        NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:kDAReturnNoResources userInfo:nil];
        completion(error);
        return;
    }

    DADiskUnmount(disk, kDADiskUnmountOptionWhole, DiskUnmountCallback, CFBridgingRetain(completion));
    CFRelease(disk);
}

static void do_plug_usb(void* p) {
    vmx_bh_delete(bh);

    NSDictionary* info = CFBridgingRelease(p);

    // pass-through this device to qemu, connect on default usb-bus
    NSString* params = [NSString stringWithFormat:@"driver=usb-host,hostaddr=0x%x,id=usb%u",
                        [info[@"locationID"] unsignedIntValue], [info[@"locationID"] unsignedIntValue]];
    QemuOpts* opts = vmx_opts_parse(vmx_find_opts("device"), [params UTF8String], 0);
    DeviceState* dev = qdev_device_add(opts);
}

static void do_unplug_usb(void* p) {
    vmx_bh_delete(bh);
    qdev_unplug((DeviceState*)p, NULL);
}

- (IBAction)usbDeviceItemDidCheck:(id)sender {

    NSDictionary* info = [sender representedObject];
    NSAssert([info isKindOfClass:[NSDictionary class]], @"Invalid represented object");

    DeviceState* usb = obj_for_usbinfo(info);
    if (usb) {
        // device is connected to qemu already, disconnect
        bh = vmx_bh_new(do_unplug_usb, usb);
        vmx_bh_schedule_idle(bh);
    } else {
        // try to unmount if it is mass storage
        [self unmountDisk:[info[@"locationID"] unsignedIntValue] completion:^(NSError *error){
            if (error) {
                NSAlert* alert = [NSAlert alertWithError:error];
                [alert beginSheetModalForWindow:self.normalWindow completionHandler:NULL];
                return;
            }

            bh = vmx_bh_new(do_plug_usb, CFBridgingRetain(info));
            vmx_bh_schedule_idle(bh);
        }];
    }
}

- (void)updateUSBMenu:(NSArray*)connectedDevices {
    NSMenu* menu = [NSMenu new];

    // remove built-in devices
    NSIndexSet* indexes = [connectedDevices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL* stop) {
        return ![obj[@"Built-In"] boolValue];
    }];
    connectedDevices = [[connectedDevices objectsAtIndexes:indexes] sortedArrayUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
        NSString* title1 = [NSString stringWithFormat: @"%@ %@ [%X]", obj1[@"USB Vendor Name"], obj1[@"USB Product Name"],
                      [obj1[@"locationID"] intValue]];
        NSString* title2 = [NSString stringWithFormat: @"%@ %@ [%X]", obj2[@"USB Vendor Name"], obj2[@"USB Product Name"],
                             [obj2[@"locationID"] intValue]];
        return [title1 compare:title2];
    }];

    if (0 == [connectedDevices count]) {
        NSMenuItem* item = [NSMenuItem new];
        item.enabled = FALSE;
        item.title = NSLocalizedString(@"No USB Devices Connected", "");
        [menu addItem:item];
    } else {
        NSMenuItem* item = [NSMenuItem new];
        item.enabled = FALSE;
        item.title = NSLocalizedString(@"Select USB Devices to Map to VM", "");
        [menu addItem:item];
        [menu addItem:[NSMenuItem separatorItem]];

        for (NSDictionary* info in connectedDevices) {
            NSMenuItem* item = [NSMenuItem new];
            item.representedObject = info;
            item.target = self;
            item.action = @selector(usbDeviceItemDidCheck:);
            item.title = [NSString stringWithFormat: @"%@ %@ [%X]", info[@"USB Vendor Name"], info[@"USB Product Name"],
                                                                    [info[@"locationID"] intValue]];
            item.state = obj_for_usbinfo(info) ? NSOnState : NSOffState;
            [menu addItem:item];
        }

        // Append preconfigured devices
        for (HWUsb* dev in self.vm.hw.usb) {
            if (nil == dev.name)
                // dummy device
                continue;

            NSUInteger index = [connectedDevices indexOfObjectPassingTest:^BOOL(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
                return [dev isEqual:obj];
            }];
            if (NSNotFound != index) {
                // device is already listed above
                continue;
            }

            NSMenuItem* item = [NSMenuItem new];
            item.representedObject = dev;
            item.title = dev.name;
            item.enabled = FALSE;
            [menu addItem:item];
        }
    }
    self.usbMenuItem.submenu = menu;
}

- (void)createMenu
{
    // Add menus
    NSMenu      *menu;
    NSMenuItem  *menuItem;
    
    [NSApp setMainMenu:[[NSMenu alloc] initWithTitle: @"VM"]];
    
    // Application menu
    menu = [[NSMenu alloc] initWithTitle:@"VM"];
    [menu addItemWithTitle:@"About VM" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""]; // About QEMU
    [menu addItem:[NSMenuItem separatorItem]]; //Separator
    [menu addItemWithTitle:@"Hide VM" action:@selector(hide:) keyEquivalent:@"h"]; //Hide QEMU
    menuItem = (NSMenuItem *)[menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"]; // Hide Others
    [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];
    [menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""]; // Show All
    [menu addItem:[NSMenuItem separatorItem]]; //Separator
    [menu addItemWithTitle:@"Quit VM" action:@selector(terminate:) keyEquivalent:@"q"];
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Apple" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:menu];
    [[NSApp mainMenu] addItem:menuItem];
    
    // View menu
    menu = [[NSMenu alloc] initWithTitle:@"View"];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"F"];
    [item setKeyEquivalentModifierMask: NSShiftKeyMask | NSCommandKeyMask];
    [menu addItem: item];
    menuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:menu];
    [[NSApp mainMenu] addItem:menuItem];

    // Commands menu
    menu = [[NSMenu alloc] initWithTitle:@"Commands"];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Pause" action:@selector(performSuspend:) keyEquivalent:@""]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Restart" action:@selector(performRestart:) keyEquivalent:@""]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Shutdown" action:@selector(performShutdown:) keyEquivalent:@""]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Send Ctrl+Alt+Del" action:@selector(sendCtrlAltDel:) keyEquivalent:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Forced Restart" action:@selector(performForcedRestart:) keyEquivalent:@""]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Forced Shutdown" action:@selector(performForcedShutdown:) keyEquivalent:@""]];
    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Install Guest Add-ons" action:@selector(performInstallAddons:) keyEquivalent:@""]];
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Commands" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:menu];
    [[NSApp mainMenu] addItem:menuItem];

    // Devices menu
    menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Devices", "")];
    item = [NSMenuItem new];
    item.submenu = menu;
    [[NSApp mainMenu] addItem:item];

    // - CDROM subitem
    if ([self.vm.hw.cdrom count] > 0) {
        self.cdromMenuItem = [NSMenuItem new];
        self.cdromMenuItem.title = NSLocalizedString(@"CD-ROM", "");
        self.cdromMenuItem.submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"CD-ROM", "")];
        [menu addItem:self.cdromMenuItem];
    }

    // - USB subitem
    self.usbMenuItem = [NSMenuItem new];
    self.usbMenuItem.title = NSLocalizedString(@"USB", "");
    self.usbMenuItem.submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"USB", "")];
    [menu addItem:self.usbMenuItem];

    // Window menu
    menu = [[NSMenu alloc] initWithTitle:@"Window"];
    [menu addItem: [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"]]; // Miniaturize
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:menu];
    [[NSApp mainMenu] addItem:menuItem];
    [NSApp setWindowsMenu:menu];
}

static void on_obj_event(DeviceState* object, int event, void* opaque)
{
    VmAppController* self = (__bridge VmAppController*)opaque;
    dispatch_async(dispatch_get_main_queue(), ^{

        if (strncmp(object->id, "usb", 3) == 0) {
            // update USB menu items
            [self updateUSBMenu:self.usbMonitor.connectedDevices];
        }

        if (strncmp(object->id, "cdrom", 5) == 0) {
            // update USB menu items
            [self updateCdMenu:nil];
        }
    });
}

static void on_vm_created(void* opaque)
{
    VmAppController* me = (__bridge VmAppController*)opaque;

    // subscribe on all changes in vm objects
    qdev_set_event_monitor(sysbus_get_default(), on_obj_event, (__bridge void *)me);

    dispatch_async(dispatch_get_main_queue(), ^{

        // do additional initializations
        [me updateUSBMenu:me.usbMonitor.connectedDevices];
        [me updateCdMenu:nil];

        // update USB menu on host devices scope change
        [me.observer observe:me.usbMonitor forKeyPath:@"connectedDevices" change:^(id newValue) {
            [me updateUSBMenu:newValue];
        }];
    });
}

- (instancetype)initWithVM:(VM*)vm
{
    COCOA_DEBUG("VmAppController: init\n");
    
    self = [super init];
    if (self) {

        self.vm = vm;

        // create USB monitor to sync state on the menu with list of devices connected
        self.usbMonitor = [USBDeviceMonitor new];

        // this is universal KVO observer
        self.observer = [ScopedValueObserver new];

        // create a window
        self.normalWindow = [[NSWindow alloc] initWithContentRect: NSMakeRect(0.0, 0.0, 640.0, 480.0)
                                                   styleMask:NSTitledWindowMask|NSMiniaturizableWindowMask|NSClosableWindowMask|NSResizableWindowMask
                                                     backing:NSBackingStoreNonretained defer:NO];
        if(!self.normalWindow) {
            fprintf(stderr, "(cocoa) can't create window\n");
            exit(1);
            
        }
        self.normalWindow.restorable = FALSE;
        self.normalWindow.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
        
        NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame: [[self.normalWindow contentView] frame]];
        [self.normalWindow setContentView: scrollView];
        
        // create a view and add it to the window
        self.vmView = [[VmView alloc] initWithFrame: scrollView.frame pixelFormat:[NSOpenGLView defaultPixelFormat]];
        // the scroll view should have both horizontal
        // and vertical scrollers
        [scrollView setHasVerticalScroller:YES];
        [scrollView setHasHorizontalScroller:YES];
        
        // configure the scroller to have no visible border
        [scrollView setBorderType:NSNoBorder];
        scrollView.scrollerStyle = NSScrollerStyleLegacy;
        [scrollView setVerticalScrollElasticity:NSScrollElasticityNone];
        [scrollView setHorizontalScrollElasticity:NSScrollElasticityNone];
        scrollView.autohidesScrollers = YES;
        
        // set the autoresizing mask so that the scroll view will
        // resize with the window
        [scrollView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        
        // set theImageView as the documentView of the scroll view
        [scrollView setContentView:  [[VmxClipView alloc] initWithFrame: scrollView.frame]];
        [scrollView setDocumentView:self.vmView];
        
        [self.normalWindow setAcceptsMouseMovedEvents:YES];
        [self.normalWindow setTitle: @"Veertu"];
       // [self.normalWindow makeKeyAndOrderFront:self];
        [self.normalWindow setDelegate: self.vmView];
        [self.normalWindow center];
    
        [self createMenu];

        self.rootController = [[NSWindowController alloc] initWithWindow: self.normalWindow];
        //[self.rootController showWindow:self];

        // perform additional initialization after VM started
        qdev_on_machine_creation_done(on_vm_created, (__bridge void *)self);
    }
    return self;
}

- (void) dealloc
{
    COCOA_DEBUG("VmAppController: dealloc\n");
}

- (void)applicationDidFinishLaunching: (NSNotification *) note
{
    COCOA_DEBUG("VmAppController: applicationDidFinishLaunching\n");

    [[NSNotificationCenter defaultCenter] addObserverForName:VmViewMouseCapturedNotification
                                                      object:self.vmView
                                                       queue:[NSOperationQueue currentQueue]
                                                  usingBlock:^(NSNotification * _Nonnull note) {

        NSString* name = [self.vm.display_name length] ? self.vm.display_name : [self.vm_name lastPathComponent];
        self.normalWindow.title = [NSString stringWithFormat:@"%@ - (%@)", name, NSLocalizedString(@"Press ctrl + option to release mouse", "")];
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:VmViewMouseReleasedNotification
                                                      object:self.vmView
                                                       queue:[NSOperationQueue currentQueue]
                                                  usingBlock:^(NSNotification * _Nonnull note) {

        NSString* name = [self.vm.display_name length] ? self.vm.display_name : [self.vm_name lastPathComponent];
        self.normalWindow.title = name;
    }];

    [self startEmulationWithArgc:gArgc argv:(char **)gArgv];
    
    if (!self.vm.advanced.headless) {
        [self.rootController showWindow:self];
        [NSApp activateIgnoringOtherApps:YES];
    }
    else
        [NSApp setActivationPolicy: NSApplicationActivationPolicyProhibited];
}

- (NSApplicationTerminateReply) applicationShouldTerminate:(NSNotification *)aNotification
{
    [self.vmView windowShouldClose: self];
    [self.normalWindow close];
    return NSTerminateLater;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    COCOA_DEBUG("VmAppController: applicationWillTerminate\n");
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return NO;
}

int _argc = 0;
char **_argv = NULL;
char **_env = NULL;

bool appStarted = false;

#define PTHREAD_HIGHEST_PRIO    31

void *veertu_main_thread(void *p)
{
    struct sched_param sp;

    VmAppController* self = CFBridgingRelease(p);

    // set highest priority for main loop thread to mitigate poll() timeouts (OSX bug?)
    memset(&sp, 0, sizeof(struct sched_param));
    sp.sched_priority = PTHREAD_HIGHEST_PRIO;
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp)  == -1) {
        printf("Failed to change priority.\n");
        return 0;
    }

    while (!appStarted)
        usleep(100);
    @autoreleasepool {
        vmx_main([self.vm.name fileSystemRepresentation], _argc, _argv, _env);
    }
    exit(0);
    return 0;
}

- (void)startEmulationWithArgc:(int)argc argv:(char**)argv
{
    COCOA_DEBUG("VmAppController: startEmulationWithArgc\n");

    if ([self.vm.display_name length])
        [self.normalWindow setTitle:self.vm.display_name];
    else
        [self.normalWindow setTitle: [self.vm_name lastPathComponent]];
    
    //status = vmx_main(argc, argv, *_NSGetEnviron());
    _argc = argc;
    _argv = argv;
    //_env = *_NSGetEnviron();
    pthread_t tid;
    pthread_create(&tid, NULL, veertu_main_thread, CFBridgingRetain(self));

    register_power_events();
    //exit(status);
}

- (void) sendCtrlAltDel: (id) sender
{
    NSLog(@"Send Ctrl+Alt+Del");
    
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_CTRL, true);
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_ALT, true);
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_DELETE, true);
    
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_CTRL, false);
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_ALT, false);
    vmx_input_event_send_key_qcode(NULL, Q_KEY_CODE_DELETE, false);
}

- (BlockDriverState*) find_vmstate_bs
{
    BlockDriverState *bs = NULL;
    while ((bs = bdrv_next(bs))) {
        if (bdrv_can_snapshot(bs)) {
            return bs;
        }
    }
    return NULL;
}

- (void) ejectCdrom: (int) cid
{
    char device[128];
    
    sprintf(device, "cdrom.%d", cid);
    qmp_eject(device, false, false, NULL);
}

void do_eject_cdrom(void *p)
{
    vmx_bh_delete(bh);

    HWCdrom* cd = (__bridge_transfer HWCdrom*)p;
    NSString* name = [NSString stringWithFormat:@"cdrom.%u", [cd.controller intValue]];
    qmp_eject([name UTF8String], false, true, NULL);
}

- (IBAction) performEjectCdrom: (id) item
{
    // work only with cdrom.0 to simplify UI
    if ([self.vm.hw.cdrom count] == 0)
        return;

    HWCdrom* cd = self.vm.hw.cdrom[0];
    cd.media_in = false;
    cd.file = nil;

    [[VMLibrary sharedVMLibrary] writeVmProperties:self.vm];

    //[self ejectCdrom: i];
    bh = vmx_bh_new(do_eject_cdrom, CFBridgingRetain(cd));
    vmx_bh_schedule_idle(bh);

    [self updateCdMenu:nil];
}

static void do_change_cdrom(void* p) {
    vmx_bh_delete(bh);

    HWCdrom* cdrom = (__bridge_transfer HWCdrom*)p;
    Error* error = NULL;
    NSString* name = [NSString stringWithFormat:@"cdrom.%u", [cdrom.controller intValue]];
    qmp_change_blockdev([name UTF8String], [cdrom.file fileSystemRepresentation], NULL, &error);
}

- (IBAction) performAttachCdrom: (id) item
{
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    NSArray *fileTypesArray = [NSArray arrayWithObjects:@"iso", nil];
    
    // Enable options in the dialog.
    [openDlg setCanChooseFiles: YES];
    [openDlg setAllowedFileTypes: fileTypesArray];
    [openDlg setAllowsMultipleSelection: FALSE];
    openDlg.prompt = NSLocalizedString(@"Attach", "");
    
    // Display the dialog box
    [openDlg beginSheetModalForWindow:self.normalWindow completionHandler:^(NSModalResponse returnCode) {
        if (NSFileHandlingPanelOKButton != returnCode)
            return;

        NSArray *files = [openDlg URLs];

        NSString *iso = [[files objectAtIndex: 0] path];
        NSLog(@"path %@", iso);

        if (!iso || ![[NSFileManager defaultManager] fileExistsAtPath:iso])
            return;

        HWCdrom *old_cd = [(NSMenuItem*)item representedObject];

        HWCdrom *cd = self.vm.hw.cdrom[0];
        cd.file = iso;
        cd.media_in = TRUE;

        [[VMLibrary sharedVMLibrary] writeVmProperties:self.vm];

        // plug in
        bh = vmx_bh_new(do_change_cdrom, CFBridgingRetain(cd));
        vmx_bh_schedule_idle(bh);

        [self updateCdMenu:nil];
    }];
}

- (void) performSuspend: (id) sender
{
    suspend_vm();
}

- (void) performRestart: (id) sender
{
    reboot_vm(false);
}

- (void) performShutdown: (id) sender
{
    shutdown_vm(false);
}

- (void) performForcedRestart: (id) sender
{
    reboot_vm(true);
}

- (void) performForcedShutdown: (id) sender
{
    shutdown_vm(true);
}

void do_add_cdrom(void *p)
{
    vmx_bh_delete(bh);
    add_cdrom();
}

- (void) performInstallAddons: (id) item
{
    bh = vmx_bh_new(do_add_cdrom, NULL);
    vmx_bh_schedule_idle(bh);

    [self.vmView ungrabMouse];
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
    [alert setMessageText:NSLocalizedString(@"Guest add-ons for Windows Vista and up", nil)];
    [alert setInformativeText:NSLocalizedString(@"To install the add-ons open the added CD-ROM, run setup.exe and follow instructions on the screen. The add-ons feature mouse integration, copy/paste between VM and OS X, file sharing and full screen support", nil)];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert runModal];

    [item setAction: nil];
}

//handler for the quit apple event
- (void)handleQuitEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)replyEvent
{
    [[NSApplication sharedApplication] terminate:self];
}

@end
