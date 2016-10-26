#ifndef _NOTIFIERS_H
#define _NOTIFIERS_H

#include "qemu/queue.h"

typedef struct Notifier {
    QLIST_ENTRY(Notifier) node;
    void (*notify)(struct Notifier *notifier, void *opaque);
} Notifier;

typedef struct VeertuNotifiers {
    QLIST_HEAD(, Notifier) notifiers;
} VeertuNotifiers;


typedef struct RetNotifier {
    QLIST_ENTRY(RetNotifier) node;
    int (*notify)(struct RetNotifier *notifier, void *opaque);
} RetNotifier;

typedef struct RetVeertuNotifiers {
    QLIST_HEAD(, RetNotifier) notifiers;
} RetVeertuNotifiers;



#define VEERTU_NOTIFIERS_INIT(head) { QLIST_HEAD_INITIALIZER(head.notifiers) }

static void inline veertu_notifiers_remove(Notifier *notifier)
{
    QLIST_REMOVE(notifier, node);
}

static void inline veertu_notifiers_init(VeertuNotifiers *notifiers)
{
    QLIST_INIT(&notifiers->notifiers);
}

static void inline veertu_notifiers_add(VeertuNotifiers *notifiers, Notifier *notifier)
{
    QLIST_INSERT_HEAD(&notifiers->notifiers, notifier, node);
}

static void inline veertu_notifiers_notify(VeertuNotifiers *notifiers, void *opaque)
{
    Notifier *notifier;
    Notifier *next;
    
    QLIST_FOREACH_SAFE(notifier, &notifiers->notifiers, node, next)
        notifier->notify(notifier, opaque);
}

static int inline veertu_notifiers_notify_break(RetVeertuNotifiers *notifiers, void *opaque)
{
    int r;
    RetNotifier *notifier;
    RetNotifier *next;
    
    QLIST_FOREACH_SAFE(notifier, &notifiers->notifiers, node, next) {
        r = notifier->notify(notifier, opaque);
        if (r)
            return r;
    }
    
    return 0;
}

#endif