#include "qemu-common.h"
#include "qemu/event_notifier.h"
#include "emuchar.h"
#include "qemu/main-loop.h"

struct EventNotifier
{
    int rfd;
    int wfd;
};

void veertu_event_notifier_init_fd(EventNotifier *e, int fd)
{
    e->rfd = fd;
    e->wfd = fd;
}

EventNotifier* veertu_event_notifier_create(int active)
{
    int fds[2];

    EventNotifier *e = (EventNotifier*)g_malloc(sizeof(EventNotifier));
    vmx_pipe(fds);
    fcntl_setfl(fds[0], O_NONBLOCK);
    fcntl_setfl(fds[1], O_NONBLOCK);
    
    e->rfd = fds[0];
    e->wfd = fds[1];

    if (active)
        veertu_event_notifier_set(e);
    return e;
}

void veertu_event_notifier_destroy(EventNotifier *e)
{
    close(e->rfd);
    close(e->wfd);
    g_free(e);
}

int veertu_event_notifier_get_fd(EventNotifier *e)
{
    return e->rfd;
}

int veertu_event_notifier_set_handler(EventNotifier *e,
                               EventNotifierHandler *handler)
{
    return vmx_set_fd_handler(e->rfd, (IOHandler *)handler, NULL, e);
}

int veertu_event_notifier_set(EventNotifier *e)
{
    static const uint64_t value = 1;
    ssize_t ret;

    do {
        ret = write(e->wfd, &value, sizeof(value));
    } while (ret < 0 && errno == EINTR);

    if (ret < 0 && errno != EAGAIN)
        return -1;
    return 0;
}

int veertu_event_notifier_test_and_clear(EventNotifier *e)
{
    int value;
    ssize_t len;
    char buffer[512];

    value = 0;
    do {
        len = read(e->rfd, buffer, sizeof(buffer));
        value |= (len > 0);
    } while ((len == -1 && errno == EINTR) || len == sizeof(buffer));

    return value;
}
