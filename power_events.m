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
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

static io_connect_t root_port; // a reference to the Root Power Domain IOService
static NSMutableArray* powerEventClients = NULL;

void notify_host_power_event(int event);

void sleep_callBack(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
{
    printf("messageType %08lx, arg %08lx\n", (long unsigned int)messageType, (long unsigned int)messageArgument);
    
    switch (messageType) {
        case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
             Applications have a chance to prevent sleep by calling IOCancelPowerChange.
             Most applications should not prevent idle sleep.
             
             Power Management waits up to 30 seconds for you to either allow or deny idle
             sleep. If you don't acknowledge this power change by calling either
             IOAllowPowerChange or IOCancelPowerChange, the system will wait 30
             seconds then go to sleep.
             */
            
            //Uncomment to cancel idle sleep
            //IOCancelPowerChange( root_port, (long)messageArgument );
            // we will allow idle sleep
            IOAllowPowerChange(root_port, (long)messageArgument);
            break;
            
        case kIOMessageSystemWillSleep:
            /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
             IOCancelPowerChange to acknowledge this message, sleep will be
             delayed by 30 seconds.
             
             NOTE: If you call IOCancelPowerChange to deny sleep it returns
             kIOReturnSuccess, however the system WILL still go to sleep.
             */
            
            notify_host_power_event(0);
            IOAllowPowerChange( root_port, (long)messageArgument );
            break;
            
        case kIOMessageSystemWillPowerOn:
            //System has started the wake up process...
            notify_host_power_event(1);
            break;
            
        case kIOMessageSystemHasPoweredOn:
            //System has finished waking up...
            break;
            
        default:
            break;
            
    }
    
}

typedef void (*power_callback)(int, void *);

typedef struct power_events_client {
    void *opaque;
    power_callback callback;
} power_events_client;

void register_host_power_event(void *opaque, power_callback callback)
{
    if (!powerEventClients)
        powerEventClients = [NSMutableArray array];
    power_events_client client = {opaque, callback};
    NSData *d = [NSData dataWithBytes: &client length: sizeof(client)];
    [powerEventClients addObject: d];
}

void notify_host_power_event(int event)
{
    if (!powerEventClients)
        return;
    for (NSData *d in powerEventClients) {
        power_events_client client;
        memcpy(&client, [d bytes], [d length]);
        client.callback(event, client.opaque);
    }
}

void register_power_events()
{
    // notification port allocated by IORegisterForSystemPower
    IONotificationPortRef  notifyPortRef;
    io_object_t            notifierObject;
    // this parameter is passed to the callback
    void*                  refCon;
    
    // register to receive system sleep notifications
    root_port = IORegisterForSystemPower(refCon, &notifyPortRef, sleep_callBack, &notifierObject);
    if (!root_port) {
        printf("IORegisterForSystemPower failed\n");
    }
    
    // add the notification port to the application runloop
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes);
}
