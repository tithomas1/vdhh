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

#import <OpenGL/gl.h>
#import <Carbon/Carbon.h>
#import "VmView.h"
#include "qemu-common.h"
#include "ui/console.h"
#include "ui/input.h"
#include "cocoa_util.h"
#include "sysemu.h"
#include "vmmanager/vmlibrary_ops.h"

extern bool appStarted;
static QEMUBH *bh = NULL;
extern DisplayChangeListener *dcl;

static BOOL cursorIsHidden = NO;

extern bool use_hdpi;

NSString* const VmViewMouseCapturedNotification = @"VmView.MouseCaptured";
NSString* const VmViewMouseReleasedNotification = @"VmView.MouseReleased";

@implementation VmView

// This is the renderer output callback function
static CVReturn MyDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
    CVReturn result;
    @autoreleasepool {
        result = [(__bridge VmView*)displayLinkContext getFrameForTime:outputTime];
    }
    return result;
}

- (void)prepareOpenGL
{
    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    
    // Create a display link capable of being used with all active displays
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    
    // Set the renderer output callback function
    CVDisplayLinkSetOutputCallback(displayLink, &MyDisplayLinkCallback, (__bridge void *)(self));
    
    // Set the display link for the current renderer
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
    
    // Activate the display link
    CVDisplayLinkStart(displayLink);
    timeFreq = CVGetHostClockFrequency();
    prevTime = 0;
    appStarted = true;
}

- (void) reshape
{
    NSRect rect;
    //[super reshape];
    
    CGLLockContext([[self openGLContext] CGLContextObj]);
    
    [[self openGLContext] makeCurrentContext];
    [[self openGLContext] update];

    NSWindow *normalWindow = self.window;
    NSScrollView *scrollView = normalWindow.contentView;
    NSSize real_size = [[scrollView contentView] documentVisibleRect].size;
    rect = [[normalWindow contentView] frame];

    rect.size = real_size;
    if (use_hdpi)
        rect = [self convertRectToBacking: rect];

    //NSLog(@"glViewport: %f %f %f %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    glViewport(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    CGLUnlockContext([[self openGLContext] CGLContextObj]);

    [self updateTracking];
}

- (void)renewGState
{
    // Called whenever graphics state updated (such as window resize)
    
    // OpenGL rendering is not synchronous with other rendering on the OSX.
    // Therefore, call disableScreenUpdatesUntilFlush so the window server
    // doesn't render non-OpenGL content in the window asynchronously from
    // OpenGL content, which could cause flickering.  (non-OpenGL content
    // includes the title bar and drawing done by the app with other APIs)
    NSWindow *normalWindow = self.window;
    [normalWindow disableScreenUpdatesUntilFlush];
    
    [super renewGState];
}



- (id)initWithFrame:(NSRect)frameRect pixelFormat:(NSOpenGLPixelFormat *)format
{
    COCOA_DEBUG("VmxCocoaView: initWithFrame:pixelFormat\n");
    
    self = [super initWithFrame:frameRect pixelFormat:format];
    if (self) {
        screenBuffer = NULL;
        screen.bitsPerComponent = 8;
        screen.bitsPerPixel = 32;
        screen.width = frameRect.size.width;
        screen.height = frameRect.size.height;
        
        displayProperties.zoom = 1.0;

        is_suspending = false;
        if (use_hdpi)
            [self setWantsBestResolutionOpenGLSurface:YES];
    }
    return self;
}

- (void) dealloc
{
    // Release the display link
    if (displayLink)
        CVDisplayLinkRelease(displayLink);

    COCOA_DEBUG("QemuCocoaView: dealloc\n");
}

- (BOOL) isOpaque
{
    return YES;
}

- (BOOL) screenContainsPoint:(NSPoint) p
{
    NSScrollView *scrollView = self.window.contentView;

    if (p.x < 0 || p.y < 0)
        return FALSE;
    NSView *v = scrollView.contentView;
    if (isFullscreen)
        v = self;
    CGPoint point = [v convertPoint:p fromView:nil];
    CGRect rect = [v  bounds];
    
    return ([v  mouse:point inRect:rect]);
    
}

- (void) hideCursor
{
    if (!cursorIsHidden) {
        [NSCursor hide];
        cursorIsHidden = YES;
    }
}

- (void) unhideCursor
{
    if (cursorIsHidden) {
        [NSCursor unhide];
        cursorIsHidden = NO;
    }
}

- (void) drawView
{
    if (!screenBuffer) {
        return;
    }

    NSWindow *normalWindow = self.window;
    NSScrollView *scrollView = normalWindow.contentView;
    NSRect docFrame = [[scrollView contentView] documentVisibleRect];
    //NSRect visibleFrame = [[normalWindow contentView] frame];
    //NSLog(@"visible: %f %f %f %f", docFrame.origin.x, docFrame.origin.y, docFrame.size.width, docFrame.size.height);
    
    [[self openGLContext] makeCurrentContext];
    CGLLockContext([[self openGLContext] CGLContextObj]);

    // remove old texture
    if( screen_tex != 0) {
        glDeleteTextures(1, &screen_tex);
    }

    screen_tex = 1;
    
    //calculate the texure rect
    int start = 0;
    unsigned char *startPointer = screenBuffer;
    
    glEnable(GL_TEXTURE_RECTANGLE_ARB); // enable rectangle textures
    
    // bind screenBuffer to texturea
    glPixelStorei(GL_UNPACK_ROW_LENGTH, screen.width); // Sets the appropriate unpacking row length for the bitmap.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Sets the byte-aligned unpacking that's needed for bitmaps that are 3 bytes per pixel.
    
    glBindTexture (GL_TEXTURE_RECTANGLE_ARB, screen_tex); // Binds the texture name to the texture target.
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Sets filtering so that it does not use a mipmap, which would be redundant for the texture rectangle extension
    
    // optimize loading of texture
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE); //
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE); // bypass OpenGL framework
    glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, (int)screen.height * screen.width * 4, &startPointer[start]); // bypass OpenGL driver
    
    glTexImage2D(
                 GL_TEXTURE_RECTANGLE_ARB,
                 0,
                 GL_RGBA,
                 screen.width,
                 screen.height,
                 0,
                 GL_BGRA,//GL_RGBA,
                 GL_UNSIGNED_INT_8_8_8_8_REV,//GL_UNSIGNED_BYTE,
                 &startPointer[start]);
    
    //NSLog(@"screen %d %d, docFrame %f %f %f %f", screen.width, screen.height, docFrame.origin.x, docFrame.origin.y, docFrame.size.width, docFrame.size.height);
    if (docFrame.origin.y < 0)
        docFrame.origin.y = 0;
    if (docFrame.origin.x < 0)
        docFrame.origin.x = 0;

    float scaleFactor = use_hdpi ? [self.window backingScaleFactor] : 1.0;
    glBegin(GL_QUADS);
    {
        glTexCoord2f(docFrame.origin.x, screen.height - docFrame.size.height * scaleFactor - docFrame.origin.y);
        glVertex2f(-1.0f, 1.0f);
        
        glTexCoord2f(docFrame.origin.x, screen.height - docFrame.origin.y);
        glVertex2f(-1.0f, -1.0f);
        
        glTexCoord2f(docFrame.origin.x + docFrame.size.width * scaleFactor, screen.height - docFrame.origin.y);
        glVertex2f(1.0f, -1.0f);
        
        glTexCoord2f(docFrame.origin.x + docFrame.size.width * scaleFactor, screen.height - docFrame.size.height * scaleFactor - docFrame.origin.y);
        glVertex2f(1.0f, 1.0f);
    }
    glEnd();
    
    glFlush();

    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void) drawRect: (NSRect) theRect
{
    //[[self openGLContext] makeCurrentContext];
    
    // We draw on a secondary thread through the display link
    // When resizing the view, -reshape is called automatically on the main
    // thread. Add a mutex around to avoid the threads accessing the context
    // simultaneously when resizing

    if (!screenBuffer) {
        [[NSColor blackColor] setFill];
        NSRectFill(theRect);
        [super drawRect: theRect];
    }
    [self drawView];
}

- (CVReturn) getFrameForTime:(const CVTimeStamp*)outputTime
{
    
    double hostTime = (double) outputTime->hostTime;
    double now = hostTime / timeFreq;
    
    // this will not update unless 1/30th of a second has passed since the last update
    if ( now < prevTime + (1.0 / GUI_REFRESH_INTERVAL_DEFAULT))
    {
        // returning NO will cause the layer to NOT be redrawn
        return kCVReturnSuccess;
    }

    if (screenBuffer)
        [self drawView];
    
    return kCVReturnSuccess;
}

- (void) setContentDimensionsForFrame:(NSRect)rect
{
    COCOA_DEBUG("QemuCocoaView: setContentDimensions %f %f\n", rect.size.width, rect.size.height);
    
    displayProperties.dx = rect.size.width / (float)screen.width;
    displayProperties.dy = rect.size.height / (float)screen.height;
    displayProperties.width = rect.size.width;
    displayProperties.height = rect.size.height;
    displayProperties.x = 0.0;
    displayProperties.y = 0.0;
}

- (void) doResize: (id) obj
{
    NSScreen *s = [NSScreen mainScreen];
    float scaleFactor = use_hdpi ? [self.window backingScaleFactor] : 1.0;
    float w = screen.width / scaleFactor;
    float h = screen.height / scaleFactor;

    //NSLog(@"doResize: %f %f, full screen %d", w, h, isFullscreen);

    /* isResize = TRUE; */
    NSSize normalWindowSize;
    normalWindowSize = NSMakeSize(w, h);

    NSWindow *normalWindow = self.window;
    NSScrollView *scrollView = self.window.contentView;
    NSRect wndFrame = [normalWindow frameRectForContentRect: NSMakeRect(0, 0, w, h)];

    // keep Window in correct aspect ratio
    if (!isFullscreen)
        [normalWindow setMaxSize: wndFrame.size];
    else
        [normalWindow setMaxSize: NSMakeSize(FLT_MAX, FLT_MAX)];
    //[normalWindow setAspectRatio: wndFrame.size];

    static bool firstTime = true;
    if (isResize || firstTime) {
        firstTime = false;

        [scrollView.documentView setFrame: NSMakeRect(0, 0, w, h)];
        wndFrame.origin = normalWindow.frame.origin;
        NSRect fullFrame = [s frame];
        if (wndFrame.size.width > fullFrame.size.width)
            wndFrame.size.width = fullFrame.size.width;

        //[self setContentDimensionsForFrame: NSMakeRect(0, 0, w , h)];
        if (!isFullscreen) {
            [normalWindow setFrame:wndFrame display:YES animate: NO];
            [normalWindow center];
        }
        else
            [normalWindow setFrame: [[NSScreen mainScreen] frame] display:YES animate: NO];

        [self updateTracking];
    }
}

- (void) switchSurface:(DisplaySurface *)surface
{
    //COCOA_DEBUG("QemuCocoaView: switchSurface\n");
    
    float w = surface_width(surface);
    float h = surface_height(surface);
    
    //NSLog(@"switchSurface: %d %d", w, h);
    /* cdx == 0 means this is our very first surface, in which case we need
     * to recalculate the content dimensions even if it happens to be the size
     * of the initial empty window.
     */
    
    isResize = (w != screen.width || h != screen.height);
    
    //sync host window color space with guests
    screen.bitsPerPixel = surface_bits_per_pixel(surface);
    screen.bitsPerComponent = surface_bytes_per_pixel(surface) * 2;
    
    //dataProviderRef = CGDataProviderCreateWithData(NULL, surface_data(surface), w * 4 * h, NULL);
    screenBuffer = surface_data(surface);
    
    // update screen state
    screen.width = w;
    screen.height = h;
    
    [self performSelectorOnMainThread:@selector(doResize:) withObject:nil waitUntilDone:FALSE];
}


BOOL wasAcceptingMouseEvents;
NSTrackingRectTag trackingRect;

- (void)viewDidMoveToWindow
{
    NSScrollView *scrollView = self.window.contentView;
    trackingRect = [self addTrackingRect:[scrollView documentVisibleRect] owner:self userData:NULL assumeInside:NO];
}

- (void)viewWillMoveToWindow: (NSWindow *)window
{
    if (!window && [self window])
        [self removeTrackingRect: trackingRect];
    [super viewWillMoveToWindow:window];
}

- (void) updateTracking
{
    NSScrollView *scrollView = self.window.contentView;
    NSSize real_size = [[scrollView contentView] documentVisibleRect].size;
    NSRect rect = [[self.window contentView] frame];
    rect = NSMakeRect(rect.origin.x, rect.origin.y, real_size.width, real_size.height);
    if (use_hdpi)
        rect = [self convertRectToBacking: rect];
    
    //NSLog(@"glViewport: %f %f %f %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    [self removeTrackingRect:trackingRect];
    trackingRect = [self addTrackingRect:[self visibleRect] owner:self userData:NULL assumeInside:NO];
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    wasAcceptingMouseEvents = [[self window] acceptsMouseMovedEvents];
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];

    if (isAbsoluteEnabled) {
        if (!isMouseGrabbed)
            [self grabMouse];
    }
}

- (void)mouseExited:(NSEvent *)theEvent
{
    [[self window] setAcceptsMouseMovedEvents:wasAcceptingMouseEvents];

    [self handleMouseButtonEvent: 0];
    if (isAbsoluteEnabled) {
        if (isMouseGrabbed)
            [self ungrabMouse];
    }
}

- (void)mouseMoved:(NSEvent *)event
{
    BOOL scroll_event = FALSE;
    NSRect r = self.frame;
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    if (use_hdpi)
        p = [self convertPointToBacking:p];
    
    if (!isMouseGrabbed)
        return;
    [NSCursor unhide];
    [NSCursor hide];

    vmx_mutex_lock_iothread();
    if (scroll_event) {
        int dy_int = (int)round([event deltaY]);
        vmx_input_queue_rel(dcl->con, INPUT_AXIS_Z, dy_int);
    }
    else if (isAbsoluteEnabled) {
        /* Note that the origin for Cocoa mouse coords is bottom left, not top left.
         * The check on screenContainsPoint is to avoid sending out of range values for
         * clicks in the titlebar.
         */
        //NSLog(@"%f %f", p.x, p.y);
        vmx_input_queue_abs(dcl->con, INPUT_AXIS_X, p.x + r.origin.x, screen.width);
        vmx_input_queue_abs(dcl->con, INPUT_AXIS_Y, screen.height - p.y - r.origin.y, screen.height);
    } else {
        vmx_input_queue_rel(dcl->con, INPUT_AXIS_X, (int)[event deltaX]);
        vmx_input_queue_rel(dcl->con, INPUT_AXIS_Y, (int)[event deltaY]);
    }
    vmx_input_event_sync();
    vmx_mutex_unlock_iothread();
}

- (void) handleMouseButtonEvent: (int) buttons
{
    if (!isMouseGrabbed)
        return;

    if (last_buttons != buttons) {
        static uint32_t bmap[INPUT_BUTTON_MAX] = {
            [INPUT_BUTTON_LEFT]       = MOUSE_EVENT_LBUTTON,
            [INPUT_BUTTON_MIDDLE]     = MOUSE_EVENT_MBUTTON,
            [INPUT_BUTTON_RIGHT]      = MOUSE_EVENT_RBUTTON,
            [INPUT_BUTTON_WHEEL_UP]   = MOUSE_EVENT_WHEELUP,
            [INPUT_BUTTON_WHEEL_DOWN] = MOUSE_EVENT_WHEELDN,
        };
        vmx_mutex_lock_iothread();
        vmx_input_update_buttons(dcl->con, bmap, last_buttons, buttons);
        vmx_mutex_unlock_iothread();
        last_buttons = buttons;

        vmx_input_event_sync();
    }
}

- (void) mouseUp:(NSEvent *)event
{
    int buttons = last_buttons;
    if ([event modifierFlags] & NSCommandKeyMask)
        buttons &= ~MOUSE_EVENT_RBUTTON;
    else
        buttons &= ~MOUSE_EVENT_LBUTTON;

    [self handleMouseButtonEvent: buttons];
}

- (void) mouseDown:(NSEvent *)event
{
    int buttons = last_buttons;
    
    if (!isMouseGrabbed)
        [self grabMouse];

    if ([event modifierFlags] & NSCommandKeyMask)
        buttons |= MOUSE_EVENT_RBUTTON;
    else
        buttons |= MOUSE_EVENT_LBUTTON;

    [self handleMouseButtonEvent: buttons];
}

- (void) rightMouseUp:(NSEvent *)event
{
    int buttons = last_buttons & ~MOUSE_EVENT_RBUTTON;
    [self handleMouseButtonEvent: buttons];
}

- (void) rightMouseDown:(NSEvent *)event
{
    int buttons = last_buttons;

    if (!isMouseGrabbed)
        [self grabMouse];
    buttons |= MOUSE_EVENT_RBUTTON;

    [self handleMouseButtonEvent: buttons];
}

- (void) otherMouseUp:(NSEvent *)event
{
    int buttons = last_buttons & ~MOUSE_EVENT_MBUTTON;
    [self handleMouseButtonEvent: buttons];
}

- (void) otherMouseDown:(NSEvent *)event
{
    int buttons = 0;
    
    if (!isMouseGrabbed)
        [self grabMouse];
    buttons |= MOUSE_EVENT_MBUTTON;
    
    [self handleMouseButtonEvent: buttons];
}

- (void) mouseDragged:(NSEvent *) event
{
    [self mouseMoved: event];
}

- (void) rightMouseDragged:(NSEvent *) event
{
    [self mouseMoved: event];
}

- (void) otherMouseDragged:(NSEvent *) event
{
    [self mouseMoved: event];
}

- (void) scrollWheel:(NSEvent *)event
{
    if (!isMouseGrabbed)
        return;

    bool isTrackpad = [event hasPreciseScrollingDeltas];

    float dy = [event scrollingDeltaY];
    if (!isTrackpad)
        dy *= 10;
    int dy_int = (int)round(dy);
    if (dy < 0 && !dy_int)
        dy_int = -1;
    if (dy > 0 && !dy_int)
        dy_int = 1;
    vmx_mutex_lock_iothread();
    if (isAbsoluteEnabled)
        vmx_input_queue_abs(dcl->con, INPUT_AXIS_Z, dy_int, screen.height);
    else
        vmx_input_queue_rel(dcl->con, INPUT_AXIS_Z, dy_int);
    vmx_input_event_sync();
    vmx_mutex_unlock_iothread();
}

- (void) keyDown:(NSEvent *) event
{
    int scancode = [event keyCode];
    int keycode = cocoa_keycode_to_qemu(scancode);
    if (KBGetLayoutType(LMGetKbdType()) == kKeyboardISO) {
        if (scancode == 50)
            keycode = 86;
        else if (scancode == 10)
            keycode = 43;
    }

    // forward command key combos to the host UI unless the mouse is grabbed
    if (!isMouseGrabbed && ([event modifierFlags] & NSCommandKeyMask))
        return;

    // full screen
    if (([event modifierFlags] & NSCommandKeyMask) && ([event modifierFlags] & NSShiftKeyMask) &&
        keycode == 0x21)
        return;
    if (!([event modifierFlags] & NSDeviceIndependentModifierFlagsMask))
        [self releaseModifiers];
    
    // handle control + alt Key Combos (ctrl+alt is reserved for QEMU)
    if (vmx_console_is_graphic(NULL)) {
        vmx_mutex_lock_iothread();
        vmx_input_event_send_key_number(dcl->con, keycode, true);
        vmx_mutex_unlock_iothread();
        
        // handlekeys for Monitor
    } else {
        int keysym = 0;
        switch([event keyCode]) {
            case 115:
                keysym = QEMU_KEY_HOME;
                break;
            case 117:
                keysym = QEMU_KEY_DELETE;
                break;
            case 119:
                keysym = QEMU_KEY_END;
                break;
            case 123:
                keysym = QEMU_KEY_LEFT;
                break;
            case 124:
                keysym = QEMU_KEY_RIGHT;
                break;
            case 125:
                keysym = QEMU_KEY_DOWN;
                break;
            case 126:
                keysym = QEMU_KEY_UP;
                break;
            default:
            {
                NSString *ks = [event characters];
                if ([ks length] > 0)
                    keysym = [ks characterAtIndex:0];
            }
        }
        if (keysym)
            kbd_put_keysym(keysym);
    }
}

- (void) keyUp:(NSEvent *) event
{
    int keycode = cocoa_keycode_to_qemu([event keyCode]);
    
    // don't pass the guest a spurious key-up if we treated this
    // command-key combo as a host UI action
    if (!isMouseGrabbed && ([event modifierFlags] & NSCommandKeyMask))
        return;

    if (vmx_console_is_graphic(NULL)) {
        vmx_mutex_lock_iothread();
        vmx_input_event_send_key_number(dcl->con, keycode, false);
        vmx_mutex_unlock_iothread();
    }
}

- (void)flagsChanged:(NSEvent *)event
{
    int keycode = cocoa_keycode_to_qemu([event keyCode]);
    if (![event keyCode]) {
        [self releaseModifiers];
        return;
    }

    if ((keycode == 219 || keycode == 220) && !isMouseGrabbed)
        /* Don't pass command key changes to guest unless mouse is grabbed */
        keycode = 0;
    if ((keycode == 219 || keycode == 220)) {
        /* remap cmd to ctrl */
        struct VMAddOnsSettings settings = {0};
        if (get_addons_settings(get_vm_folder(), &settings) && settings.remap_cmd)
            keycode = 29;
    }

    if (keycode) {
        if (keycode == 58 || keycode == 69) { // emulate caps lock and num lock keydown and keyup
            vmx_mutex_lock_iothread();
            vmx_input_event_send_key_number(dcl->con, keycode, true);
            vmx_input_event_send_key_number(dcl->con, keycode, false);
            vmx_mutex_unlock_iothread();
        } else if (vmx_console_is_graphic(NULL)) {
            if (modifiers_state[keycode] == 0 && [event modifierFlags] & NSDeviceIndependentModifierFlagsMask) {
                // keydown
                vmx_mutex_lock_iothread();
                vmx_input_event_send_key_number(dcl->con, keycode, true);
                vmx_mutex_unlock_iothread();
                modifiers_state[keycode] = 1;
            } else { // keyup
                vmx_mutex_lock_iothread();
                vmx_input_event_send_key_number(dcl->con, keycode, false);
                vmx_mutex_unlock_iothread();
                modifiers_state[keycode] = 0;
            }
        }
    }

    // release Mouse grab when pressing ctrl+alt
    if (([event modifierFlags] & NSControlKeyMask) && ([event modifierFlags] & NSAlternateKeyMask))
        [self ungrabMouse];
}

- (void) releaseModifiers
{
    // unset all pressed key
    for (int i = 0; i < ARRAY_SIZE(modifiers_state); i++) {
        if (modifiers_state[i]) {
            //NSLog(@"i %d\n", i);
            vmx_input_event_send_key_number(dcl->con, i, false);
            modifiers_state[i] = 0;
        }
    }
}

- (void) grabMouse
{
    NSWindow *normalWindow = self.window;
    COCOA_DEBUG("QemuCocoaView: grabMouse\n");

    last_buttons = 0;
    if (!isFullscreen && !isAbsoluteEnabled)
        [[NSNotificationCenter defaultCenter] postNotificationName:VmViewMouseCapturedNotification object:self];

    [self hideCursor];
    if (!isAbsoluteEnabled) {
        isMouseDeassociated = TRUE;
        CGAssociateMouseAndMouseCursorPosition(FALSE);
    }
    isMouseGrabbed = TRUE; // while isMouseGrabbed = TRUE, QemuCocoaApp sends all events to [cocoaView handleEvent:]
    NSPasteboard*  myPasteboard  = [NSPasteboard generalPasteboard];
    NSString* myString = [myPasteboard  stringForType:NSPasteboardTypeString];
    vmx_mutex_lock_iothread();
    vmx_grab_mouse([myString UTF8String]);

    [self releaseModifiers];
    vmx_mutex_unlock_iothread();
}

- (void) ungrabMouse
{
    NSWindow *normalWindow = self.window;
    COCOA_DEBUG("QemuCocoaView: ungrabMouse\n");

    // clean mouse state
    [self handleMouseButtonEvent: 0];
    if (!isFullscreen && !isAbsoluteEnabled) {
        [[NSNotificationCenter defaultCenter] postNotificationName:VmViewMouseReleasedNotification object:self];
    }
    [self unhideCursor];
    if (isMouseDeassociated) {
        CGAssociateMouseAndMouseCursorPosition(TRUE);
        isMouseDeassociated = FALSE;
    }
    isMouseGrabbed = FALSE;
    vmx_mutex_lock_iothread();
    vmx_ungrab_mouse();
    [self releaseModifiers];
    vmx_mutex_unlock_iothread();
}

void cocoa_set_clipboard(char *str)
{
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    
    [pasteBoard setString:[NSString stringWithUTF8String:str] forType:NSStringPboardType];
}

- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize
{
    COCOA_DEBUG("QemuCocoaView: windowWillResize: toSize: NSSize(%f, %f)\n", proposedFrameSize.width, proposedFrameSize.height);
    //vmx_set_res(proposedFrameSize.width, proposedFrameSize.height);
    [self updateTracking];

    //[normalWindow disableScreenUpdatesUntilFlush];
    return proposedFrameSize;
}

void do_suspendVM(void *param)
{
    vmx_bh_delete(bh);
    bh = NULL;
    suspend_vm_internal();

    [NSApp replyToApplicationShouldTerminate: YES];
    
}

- (BOOL)windowShouldClose:(id)sender
{
    NSWindow *normalWindow = self.window;

    if (is_suspending)
        return YES;

    if (!runstate_is_running()) {
        //vmx_system_powerdown_request();
        // kill vmx thread and exit
        exit(0);
        return YES;
    }

    CVDisplayLinkRelease(displayLink);
    displayLink = NULL;

    is_suspending = true;
    bh = vmx_bh_new(do_suspendVM, NULL);
    vmx_bh_schedule_idle(bh);

    return YES;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    NSScrollView *scrollView = self.window.contentView;

    scrollView.backgroundColor = [NSColor blackColor];
    [self.window setMaxSize: NSMakeSize(FLT_MAX, FLT_MAX)];

    NSSize r = [self convertRectToBacking: [[NSScreen mainScreen] frame]].size;
    if (!use_hdpi)
        r = [[NSScreen mainScreen] frame].size;
    vmx_set_res((int)r.width, (int)r.height);
    
    [self.window center];
    before_full_heigt = (int)screen.height;
    before_full_width = (int)screen.width;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    //normalWindow.styleMask = NSFullScreenWindowMask;
    //NSLog(@"windowDidEnterFullScreen");
    isFullscreen = TRUE;
    //NSSize max_size = [[NSScreen mainScreen] visibleFrame].size;
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    vmx_set_res(before_full_width,before_full_heigt);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    //normalWindow.styleMask = NSTitledWindowMask|NSMiniaturizableWindowMask|NSClosableWindowMask|NSResizableWindowMask;
    NSScrollView *scrollView = self.window.contentView;
    isFullscreen = FALSE;
    
    NSRect wndFrame = [self.window frameRectForContentRect: NSMakeRect(0, 0, screen.width, screen.height)];
    
    // keep Window in correct aspect ratio
    [self.window setMaxSize: wndFrame.size];
    [self.window setContentSize: [scrollView.documentView frame].size];
    [self.window center];
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    //[[self layer] setContentsScale:[[self window] backingScaleFactor]];
    [self reshape];
    isResize = TRUE;
    [self performSelectorOnMainThread:@selector(doResize:) withObject:nil waitUntilDone:FALSE];
}

- (void) setAbsoluteEnabled:(BOOL)tIsAbsoluteEnabled {isAbsoluteEnabled = tIsAbsoluteEnabled;}
- (BOOL) isMouseGrabbed {return isMouseGrabbed;}
- (BOOL) isAbsoluteEnabled {return isAbsoluteEnabled;}
- (BOOL) isMouseDeassociated {return isMouseDeassociated;}
- (QEMUScreen) gscreen {return screen;}
- (COCOADisplayProperties) displayProperties {return displayProperties;}

@end
