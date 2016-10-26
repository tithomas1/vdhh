/*
 * QEMU Cocoa CG display driver
 *
 * Copyright (c) 2008 Mike Kronenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <AvailabilityMacros.h>

#import <Cocoa/Cocoa.h>
#import "VmxClipView.h"
#include <crt_externs.h>

#include "qemu-common.h"
#include "ui/console.h"
#include "ui/input.h"
#include "sysemu.h"
#include "snapshot.h"
#import "VmView.h"
#import "VmApp.h"
#import "cocoa_util.h"

#include "hw.h"
#include "boards.h"
#include "emublock-backend.h"
#include "emublockdev.h"
#include "qemu/config-file.h"
#include "monitor/qdev.h"

@import CoreVideo;

static bool vm_change_state_handler_added = 0;
static bool powerdown_requested = false;
static bool reboot_requested = false;

#define cgrect(nsrect) (*(CGRect *)&(nsrect))

#define titleBarHeight 0/*21.0*/

VmAppController *appController;
DisplayChangeListener *dcl;

int gArgc = 0;
char **gArgv = NULL;
bool use_hdpi = false;

// keymap conversion
int keymap[] =
{
//  SdlI    macI    macH    SdlH    104xtH  104xtC  sdl
    30, //  0       0x00    0x1e            A       QZ_a
    31, //  1       0x01    0x1f            S       QZ_s
    32, //  2       0x02    0x20            D       QZ_d
    33, //  3       0x03    0x21            F       QZ_f
    35, //  4       0x04    0x23            H       QZ_h
    34, //  5       0x05    0x22            G       QZ_g
    44, //  6       0x06    0x2c            Z       QZ_z
    45, //  7       0x07    0x2d            X       QZ_x
    46, //  8       0x08    0x2e            C       QZ_c
    47, //  9       0x09    0x2f            V       QZ_v
    41, //  10      0x0A    0x29            \       QZ_BACKQUOTE
    48, //  11      0x0B    0x30            B       QZ_b
    16, //  12      0x0C    0x10            Q       QZ_q
    17, //  13      0x0D    0x11            W       QZ_w
    18, //  14      0x0E    0x12            E       QZ_e
    19, //  15      0x0F    0x13            R       QZ_r
    21, //  16      0x10    0x15            Y       QZ_y
    20, //  17      0x11    0x14            T       QZ_t
    2,  //  18      0x12    0x02            1       QZ_1
    3,  //  19      0x13    0x03            2       QZ_2
    4,  //  20      0x14    0x04            3       QZ_3
    5,  //  21      0x15    0x05            4       QZ_4
    7,  //  22      0x16    0x07            6       QZ_6
    6,  //  23      0x17    0x06            5       QZ_5
    13, //  24      0x18    0x0d            =       QZ_EQUALS
    10, //  25      0x19    0x0a            9       QZ_9
    8,  //  26      0x1A    0x08            7       QZ_7
    12, //  27      0x1B    0x0c            -       QZ_MINUS
    9,  //  28      0x1C    0x09            8       QZ_8
    11, //  29      0x1D    0x0b            0       QZ_0
    27, //  30      0x1E    0x1b            ]       QZ_RIGHTBRACKET
    24, //  31      0x1F    0x18            O       QZ_o
    22, //  32      0x20    0x16            U       QZ_u
    26, //  33      0x21    0x1a            [       QZ_LEFTBRACKET
    23, //  34      0x22    0x17            I       QZ_i
    25, //  35      0x23    0x19            P       QZ_p
    28, //  36      0x24    0x1c            ENTER   QZ_RETURN
    38, //  37      0x25    0x26            L       QZ_l
    36, //  38      0x26    0x24            J       QZ_j
    40, //  39      0x27    0x28            '       QZ_QUOTE
    37, //  40      0x28    0x25            K       QZ_k
    39, //  41      0x29    0x27            ;       QZ_SEMICOLON
    43, //  42      0x2A    0x2b            \       QZ_BACKSLASH
    51, //  43      0x2B    0x33            ,       QZ_COMMA
    53, //  44      0x2C    0x35            /       QZ_SLASH
    49, //  45      0x2D    0x31            N       QZ_n
    50, //  46      0x2E    0x32            M       QZ_m
    52, //  47      0x2F    0x34            .       QZ_PERIOD
    15, //  48      0x30    0x0f            TAB     QZ_TAB
    57, //  49      0x31    0x39            SPACE   QZ_SPACE
    // Use `~ instead of <>
    /*86*/41, //  50      0x32    0x56	        `       QZ_BACKQUOTE2
    14, //  51      0x33    0x0e            BKSP    QZ_BACKSPACE
    0,  //  52      0x34    Undefined
    1,  //  53      0x35    0x01            ESC     QZ_ESCAPE
    220, // 54      0x36    0xdc    E0,5C   R GUI   QZ_RMETA
    219, // 55      0x37    0xdb    E0,5B   L GUI   QZ_LMETA
    42, //  56      0x38    0x2a            L SHFT  QZ_LSHIFT
    58, //  57      0x39    0x3a            CAPS    QZ_CAPSLOCK
    56, //  58      0x3A    0x38            L ALT   QZ_LALT
    29, //  59      0x3B    0x1d            L CTRL  QZ_LCTRL
    54, //  60      0x3C    0x36            R SHFT  QZ_RSHIFT
    184,//  61      0x3D    0xb8    E0,38   R ALT   QZ_RALT
    157,//  62      0x3E    0x9d    E0,1D   R CTRL  QZ_RCTRL
    0,  //  63      0x3F    Undefined
    0,  //  64      0x40    Undefined
    83, //  65      0x41                            Keypad .
    0,  //  66      0x42    Undefined
    55, //  67      0x43    0x37            KP *    QZ_KP_MULTIPLY
    0,  //  68      0x44    Undefined
    78, //  69      0x45    0x4e            KP +    QZ_KP_PLUS
    0,  //  70      0x46    Undefined
    69, //  71      0x47    0x45            NUM     QZ_NUMLOCK
    0,  //  72      0x48    Undefined
    0,  //  73      0x49    Undefined
    0,  //  74      0x4A    Undefined
    181,//  75      0x4B    0xb5    E0,35   KP /    QZ_KP_DIVIDE
    152,//  76      0x4C    0x9c    E0,1C   KP EN   QZ_KP_ENTER
    0,  //  77      0x4D    undefined
    74, //  78      0x4E    0x4a            KP -    QZ_KP_MINUS
    0,  //  79      0x4F    Undefined
    0,  //  80      0x50    Undefined
    13, //  81      0x51                            QZ_KP_EQUALS
    82, //  82      0x52    0x52            KP 0    QZ_KP0
    79, //  83      0x53    0x4f            KP 1    QZ_KP1
    80, //  84      0x54    0x50            KP 2    QZ_KP2
    81, //  85      0x55    0x51            KP 3    QZ_KP3
    75, //  86      0x56    0x4b            KP 4    QZ_KP4
    76, //  87      0x57    0x4c            KP 5    QZ_KP5
    77, //  88      0x58    0x4d            KP 6    QZ_KP6
    71, //  89      0x59    0x47            KP 7    QZ_KP7
    0,  //  90      0x5A    Undefined
    72, //  91      0x5B    0x48            KP 8    QZ_KP8
    73, //  92      0x5C    0x49            KP 9    QZ_KP9
    43, //  93      0x2A    0x2b            \       QZ_BACKSLASH   (YEN)
    0,  //  94      0x5E    Undefined
    51, //  95      0x2B    0x33            ,       QZ_COMMA       (kVK_JIS_KeypadComma)
    63, //  96      0x60    0x3f            F5      QZ_F5
    64, //  97      0x61    0x40            F6      QZ_F6
    65, //  98      0x62    0x41            F7      QZ_F7
    61, //  99      0x63    0x3d            F3      QZ_F3
    66, //  100     0x64    0x42            F8      QZ_F8
    67, //  101     0x65    0x43            F9      QZ_F9
    123,  //  102     0x66    Undefined                             (kVK_JIS_Eisu)
    87, //  103     0x67    0x57            F11     QZ_F11
    112,  //  104     0x68    Undefined                             (kVK_JIS_Kana)
    183,//  105     0x69    0xb7                    QZ_PRINT
    0,  //  106     0x6A    Undefined
    70, //  107     0x6B    0x46            SCROLL  QZ_SCROLLOCK
    0,  //  108     0x6C    Undefined
    68, //  109     0x6D    0x44            F10     QZ_F10
    0,  //  110     0x6E    Undefined
    88, //  111     0x6F    0x58            F12     QZ_F12
    0,  //  112     0x70    Undefined
    110,//  113     0x71    0x0                     QZ_PAUSE
    210,//  114     0x72    0xd2    E0,52   INSERT  QZ_INSERT
    199,//  115     0x73    0xc7    E0,47   HOME    QZ_HOME
    201,//  116     0x74    0xc9    E0,49   PG UP   QZ_PAGEUP
    211,//  117     0x75    0xd3    E0,53   DELETE  QZ_DELETE
    62, //  118     0x76    0x3e            F4      QZ_F4
    207,//  119     0x77    0xcf    E0,4f   END     QZ_END
    60, //  120     0x78    0x3c            F2      QZ_F2
    209,//  121     0x79    0xd1    E0,51   PG DN   QZ_PAGEDOWN
    59, //  122     0x7A    0x3b            F1      QZ_F1
    203,//  123     0x7B    0xcb    e0,4B   L ARROW QZ_LEFT
    205,//  124     0x7C    0xcd    e0,4D   R ARROW QZ_RIGHT
    208,//  125     0x7D    0xd0    E0,50   D ARROW QZ_DOWN
    200,//  126     0x7E    0xc8    E0,48   U ARROW QZ_UP
/* completed according to http://www.libsdl.org/cgi/cvsweb.cgi/SDL12/src/video/quartz/SDL_QuartzKeys.h?rev=1.6&content-type=text/x-cvsweb-markup */

/* Additional 104 Key XP-Keyboard Scancodes from http://www.computer-engineering.org/ps2keyboard/scancodes1.html */
/*
    221 //          0xdd            e0,5d   APPS
        //              E0,2A,E0,37         PRNT SCRN
        //              E1,1D,45,E1,9D,C5   PAUSE
    83  //          0x53    0x53            KP .
// ACPI Scan Codes
    222 //          0xde            E0, 5E  Power
    223 //          0xdf            E0, 5F  Sleep
    227 //          0xe3            E0, 63  Wake
// Windows Multimedia Scan Codes
    153 //          0x99            E0, 19  Next Track
    144 //          0x90            E0, 10  Previous Track
    164 //          0xa4            E0, 24  Stop
    162 //          0xa2            E0, 22  Play/Pause
    160 //          0xa0            E0, 20  Mute
    176 //          0xb0            E0, 30  Volume Up
    174 //          0xae            E0, 2E  Volume Down
    237 //          0xed            E0, 6D  Media Select
    236 //          0xec            E0, 6C  E-Mail
    161 //          0xa1            E0, 21  Calculator
    235 //          0xeb            E0, 6B  My Computer
    229 //          0xe5            E0, 65  WWW Search
    178 //          0xb2            E0, 32  WWW Home
    234 //          0xea            E0, 6A  WWW Back
    233 //          0xe9            E0, 69  WWW Forward
    232 //          0xe8            E0, 68  WWW Stop
    231 //          0xe7            E0, 67  WWW Refresh
    230 //          0xe6            E0, 66  WWW Favorites
*/
};

int cocoa_keycode_to_qemu(int keycode)
{
    if (ARRAY_SIZE(keymap) <= keycode) {
        fprintf(stderr, "(cocoa) warning unknown keycode 0x%x\n", keycode);
        return 0;
    }
    return keymap[keycode];
}

void vm_change_state(void *opaque, int running, RunState state)
{
    if (RUN_STATE_SHUTDOWN == state) {
        if (reboot_requested) {
            //printf("reset\n");
            runstate_set(RUN_STATE_PAUSED);
            vmx_system_reset_request();
            vm_start();
        }
        powerdown_requested = false;
        reboot_requested = false;
        no_shutdown = 0;
    }
}

#pragma mark qemu
static void cocoa_update(DisplayChangeListener *dcl,
                         int x, int y, int w, int h)
{
#if 0
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    //COCOA_DEBUG("vmx_cocoa: cocoa_update\n");

    NSRect rect;
    rect = NSMakeRect(
        x * [cocoaView displayProperties].dx,
        ([cocoaView gscreen].height - y - h) * [cocoaView displayProperties].dy,
        w * [cocoaView displayProperties].dx,
        h * [cocoaView displayProperties].dy);

    [cocoaView setNeedsDisplayInRect:rect];

    [pool release];
#endif
    //[cocoaView setNeedsDisplay:YES];
}

static void cocoa_switch(DisplayChangeListener *dcl,
                         DisplaySurface *surface)
{
    COCOA_DEBUG("vmx_cocoa: cocoa_switch\n");
    [appController.vmView switchSurface:surface];
}

static void cocoa_refresh(DisplayChangeListener *dcl)
{
    COCOA_DEBUG("vmx_cocoa: cocoa_refresh\n");

    dispatch_async(dispatch_get_main_queue(), ^{
        if (vmx_input_is_absolute()) {
            if (![appController.vmView isAbsoluteEnabled]) {
                if ([appController.vmView isMouseGrabbed]) {
                    [appController.vmView ungrabMouse];
                }
            }
        [appController.vmView setAbsoluteEnabled:YES];
        } else {
            [appController.vmView setAbsoluteEnabled:NO];
        }
    });

    CGLLockContext([[appController.vmView openGLContext] CGLContextObj]);
    graphic_hw_update(NULL);
    CGLUnlockContext([[appController.vmView openGLContext] CGLContextObj]);
}

static void cocoa_cleanup(void)
{
    COCOA_DEBUG("vmx_cocoa: cocoa_cleanup\n");
    g_free(dcl);
}

static const DisplayChangeListenerOps dcl_ops = {
    .dpy_name          = "cocoa",
    .dpy_gfx_update = cocoa_update,
    .dpy_gfx_switch = cocoa_switch,
    .dpy_refresh = cocoa_refresh,
};

void cocoa_display_init(DisplayState *ds, int full_screen)
{
    COCOA_DEBUG("vmx_cocoa: cocoa_display_init\n");

    dcl = g_malloc0(sizeof(DisplayChangeListener));

    // register vga output callbacks
    dcl->ops = &dcl_ops;
    register_displaychangelistener(dcl);

    // register cleanup function
    atexit(cocoa_cleanup);
}

extern int no_shutdown;

void shutdown_vm(bool forced)
{
    if (!runstate_is_running() || reboot_requested || powerdown_requested)
        forced = true;

    if (forced) {
        vmx_system_shutdown_request();
        powerdown_requested = false;
        return;
    }

    powerdown_requested = true;
    vmx_system_powerdown_request();
}

void reboot_vm(bool forced)
{
    if (!runstate_is_running() || reboot_requested || powerdown_requested)
        forced = true;
    no_shutdown = 1;

    if (forced) {
        vmx_system_reset_request();
        reboot_requested = false;
        no_shutdown = 0;
        return;
    }

    if (!vm_change_state_handler_added)
        vmx_add_vm_change_state_handler(vm_change_state, NULL);
    vm_change_state_handler_added = true;

    reboot_requested = true;
    vmx_system_powerdown_request();
}


void reset_vm()
{
    powerdown_requested = false;
    reboot_requested = false;
    no_shutdown = 0;

    vmx_system_reset_request();
}

void suspend_vm()
{
    [[NSApplication sharedApplication] terminate: nil];
}

void suspend_vm_internal()
{
    powerdown_requested = false;
    reboot_requested = false;
    no_shutdown = 0;

    // unplug devices if any
    unplug_cdrom();

    vm_stop(RUN_STATE_SAVE_VM);

    QDict *d = qdict_new();
    qdict_put_obj(d, "name", QOBJECT(qstring_from_str("last_state")));
    do_savevm(NULL, d);
    vmx_system_shutdown_request();
    QDECREF(d);
}

float get_screen_scale()
{
    if (!use_hdpi)
        return 1.0;
    return [[NSScreen mainScreen] backingScaleFactor];
}

void get_screen_relosution(int *w, int *h)
{
    NSRect r = [[NSScreen mainScreen] frame];
    if (use_hdpi)
        r = [[NSScreen mainScreen] convertRectToBacking: r];
    *h = (int)r.size.height;
    *w = (int)r.size.width;
}

void get_initial_screen_size(int *w, int *h)
{
    NSScreen *s = [NSScreen mainScreen];
    float scaleFactor = [s backingScaleFactor];
    if (!use_hdpi)
        scaleFactor = 1.0;

    *w = 640.0 * scaleFactor;
    *h = 480.0 * scaleFactor;
}

static DeviceState* addons_cdrom = NULL;

void add_cdrom()
{
    QemuOpts *opts;
    DriveInfo *dinfo;
    MachineClass *mc;

    if (addons_cdrom) {
        //unplug_cdrom();
        return;
    }

    // add-ons path
    NSString *root = [[NSBundle mainBundle] bundlePath];
    NSString *addons_path;
    if ([root hasSuffix: @".app"])
        addons_path = [NSString stringWithFormat:@"%@/Contents/SharedSupport/add-ons", root];
    else
        addons_path = [NSString stringWithFormat:@"%@/add-ons", root];

    NSString *drive_str = [NSString stringWithFormat: @"if=none,id=usb-cdrom,media=cdrom,file=%@/tools.iso", addons_path];
    opts = drive_def([drive_str UTF8String]);
    if (!opts)
        return;

    mc = MACHINE_GET_CLASS(current_machine);
    dinfo = drive_new(opts, mc->block_default_type);
    if (!dinfo) {
        vmx_opts_del(opts);
        return;
    }

    opts = vmx_opts_parse(vmx_find_opts("device"), "driver=usb-storage,id=usb-cdrom,drive=usb-cdrom", 0);
    addons_cdrom = qdev_device_add(opts);
}

void qdev_unplug(DeviceState *dev, Error **errp);

void unplug_cdrom()
{
    if (addons_cdrom)
        qdev_unplug(addons_cdrom, NULL);
    addons_cdrom = NULL;
}

void displayAlert(char *title, char *msg, char *button)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [NSAlert alertWithMessageText:[NSString stringWithUTF8String: title]
                                defaultButton: [NSString stringWithUTF8String: button] alternateButton: nil
                                otherButton:nil informativeTextWithFormat: [NSString stringWithUTF8String: msg]];
        [alert runModal];
    });
}

void alert_old_tools_ver()
{
    [[NSOperationQueue mainQueue] addOperationWithBlock:^ {
        NSAlert *alert = [NSAlert alertWithMessageText: NSLocalizedString(@"NEWER_TOOLS_AVAIL", nil)
                        defaultButton: NSLocalizedString(@"Close", nil) alternateButton: nil
                        otherButton:nil informativeTextWithFormat: NSLocalizedString(@"NEWER_TOOLS_INFO", nil)];
        [alert runModal];
    }];
}
