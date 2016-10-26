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

#import <Cocoa/Cocoa.h>
#include "ui/console.h"

typedef struct {
    int width;
    int height;
    int bitsPerComponent;
    int bitsPerPixel;
} QEMUScreen;

typedef struct {
    float x;
    float y;
    float width;
    float height;
    float dx;
    float dy;
    float zoom;
} COCOADisplayProperties;

extern NSString* const VmViewMouseCapturedNotification;
extern NSString* const VmViewMouseReleasedNotification;

@interface VmView : NSOpenGLView <NSWindowDelegate>
{
    CVDisplayLinkRef displayLink;

    double timeFreq;
    double prevTime;

    int last_buttons;

    bool is_suspending;
    unsigned char *screenBuffer;

    QEMUScreen screen;
    int before_full_width;
    int before_full_heigt;
    COCOADisplayProperties displayProperties;
//    NSWindow *fullScreenWindow;
    GLuint screen_tex;
    int modifiers_state[256];
    BOOL isMouseGrabbed;
    BOOL isFullscreen;
    BOOL isAbsoluteEnabled;
    BOOL isMouseDeassociated;
    
    NSProgressIndicator *progress;
    BOOL isResizing;
    BOOL isResize;
}
- (void) setContentDimensionsForFrame:(NSRect)rect;
- (void) switchSurface:(DisplaySurface *)surface;
- (void) grabMouse;
- (void) ungrabMouse;
- (void) setAbsoluteEnabled:(BOOL)tIsAbsoluteEnabled;
/* The state surrounding mouse grabbing is potentially confusing.
 * isAbsoluteEnabled tracks vmx_input_is_absolute() [ie "is the emulated
 *   pointing device an absolute-position one?"], but is only updated on
 *   next refresh.
 * isMouseGrabbed tracks whether GUI events are directed to the guest;
 *   it controls whether special keys like Cmd get sent to the guest,
 *   and whether we capture the mouse when in non-absolute mode.
 * isMouseDeassociated tracks whether we've told MacOSX to disassociate
 *   the mouse and mouse cursor position by calling
 *   CGAssociateMouseAndMouseCursorPosition(FALSE)
 *   (which basically happens if we grab in non-absolute mode).
 */
- (BOOL) isMouseGrabbed;
- (BOOL) isAbsoluteEnabled;
- (BOOL) isMouseDeassociated;
- (QEMUScreen) gscreen;
- (COCOADisplayProperties) displayProperties;

@end
