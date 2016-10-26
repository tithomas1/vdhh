#ifndef __VEERTU_EVENT_NOTIFIER_H__
#define __VEERTU_EVENT_NOTIFIER_H__

#include "qemu-common.h"

typedef struct EventNotifier EventNotifier;
typedef void EventNotifierHandler(EventNotifier *);

EventNotifier* veertu_event_notifier_create(int active);
void veertu_event_notifier_destroy(EventNotifier *);
int veertu_event_notifier_set(EventNotifier *);
int veertu_event_notifier_test_and_clear(EventNotifier *);
int veertu_event_notifier_set_handler(EventNotifier *, EventNotifierHandler *);

int veertu_event_notifier_get_fd(EventNotifier *);

#endif
