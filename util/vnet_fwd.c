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

#include "config-host.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>

#include "qemu/sockets.h"
#include "net/net.h"
#include "clients.h"
#include "monitor/monitor.h"
#include "sysemu.h"
#include "qemu-common.h"

#include "vnet_fwd.h"

#define MAX_PORT_FWD    128

typedef struct PortFwd
{
    struct in_addr host_addr;
    int host_port;
    struct in_addr guest_addr;
    int guest_port;
    int is_udp;
} PortFwd;

typedef struct PortFwdState {
    int used;
    PortFwd port_fwd;
    int listen_fd;

    int redir_fd;
    int redir_fd_connected;
    int client_fd;
} PortFwdState;

static PortFwdState port_fwd_states[MAX_PORT_FWD];

static void net_socket_accept(void *opaque);

extern uint32_t vm_ip_address;

static int get_str_sep(char *buf, int buf_size, const char **pp, int sep)
{
    const char *p, *p1;
    int len;
    p = *pp;
    p1 = strchr(p, sep);
    if (!p1)
        return -1;
    len = p1 - p;
    p1++;
    if (buf_size > 0) {
        if (len > buf_size - 1)
            len = buf_size - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
    }
    *pp = p1;
    return 0;
}

static int parse_port_fwd(const char *redir_str, PortFwd *port_fwd)
{
    const char *p;
    char buf[256];
    char *end;

    port_fwd->host_addr.s_addr = INADDR_ANY;
    port_fwd->guest_addr.s_addr = 0;
    
    p = redir_str;
    if (!p || get_str_sep(buf, sizeof(buf), &p, ':') < 0) {
        goto fail_syntax;
    }
    if (!strcmp(buf, "tcp") || buf[0] == '\0') {
        port_fwd->is_udp = 0;
    } else if (!strcmp(buf, "udp")) {
        port_fwd->is_udp = 1;
    } else {
        goto fail_syntax;
    }
    
    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0)
        goto fail_syntax;
    if (buf[0] != '\0' && !inet_aton(buf, &port_fwd->host_addr))
        goto fail_syntax;
    
    if (get_str_sep(buf, sizeof(buf), &p, '-') < 0)
        goto fail_syntax;

    port_fwd->host_port = strtol(buf, &end, 0);
    if (*end != '\0' || port_fwd->host_port < 1 || port_fwd->host_port > 65535) {
        goto fail_syntax;
    }
    
    if (get_str_sep(buf, sizeof(buf), &p, ':') < 0) {
        goto fail_syntax;
    }
    if (buf[0] != '\0' && !inet_aton(buf, &port_fwd->guest_addr)) {
        goto fail_syntax;
    }
    
    port_fwd->guest_port = strtol(p, &end, 0);
    if (*end != '\0' || port_fwd->guest_port < 1 || port_fwd->guest_port > 65535) {
        goto fail_syntax;
    }

    return 0;
    
fail_syntax:
    error_report("invalid host forwarding rule '%s'", redir_str);
    return -1;
}

static void vnet_free_fwd_state(PortFwdState *s, int close_listen)
{
    printf("closing conn\n");
    /*if (s->listen_fd > 0) {
        close(s->listen_fd);
        vmx_set_fd_handler(s->listen_fd, NULL, NULL, NULL);
    }*/
    if (s->redir_fd > 0) {
        close(s->redir_fd);
        vmx_set_fd_handler(s->redir_fd, NULL, NULL, NULL);
        s->redir_fd = 0;
    }
    if (s->client_fd > 0) {
        close(s->client_fd);
        vmx_set_fd_handler(s->client_fd, NULL, NULL, NULL);
        s->client_fd = 0;
    }
    if (close_listen) {
        close(s->listen_fd);
        vmx_set_fd_handler(s->listen_fd, NULL, NULL, NULL);
        s->listen_fd = 0;
        s->used = 0;
    }
}

static int do_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
    int res;
    do {
        res = poll(fds, nfds, timeout);
    } while (res < 0 && (errno == EAGAIN || errno == EINTR));
    return res;
}

static ssize_t do_send(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t res;
    do {
        res = vmx_send(socket, buffer, length, flags);
    } while (res < 0 && (errno == EAGAIN || errno == EINTR));
    return res;
}

static ssize_t do_recv(int socket, void *buffer, size_t length, int flags)
{
    ssize_t res;
    do {
        res = vmx_recv(socket, buffer, length, flags);
    } while (res < 0 && (errno == EAGAIN || errno == EINTR));
    return res;
}

void *redirect_func(void *opaque)
{
    PortFwdState *s = opaque;
    char buf[4096], *pbuf;
    struct pollfd fds[2];
    ssize_t size, sent;
    int res;
    
    vmx_set_nonblock(s->redir_fd);
    vmx_set_nonblock(s->client_fd);

    fds[0].fd = s->redir_fd;
    fds[1].fd = s->client_fd;

    fds[0].events = POLLIN;
    fds[1].events = POLLIN;

    while (1) {
        res = do_poll(fds, 2, 0);
        if (!res)
            continue;
        if (res < 0)
            break;

        if (fds[0].revents & (POLLERR | POLLHUP))
            break;
        if (fds[1].revents & (POLLERR | POLLHUP))
            break;

        if (fds[0].revents & POLLIN) {
            size = do_recv(fds[0].fd, buf, sizeof(buf), 0);
            if (size < 0)
                break;
    
            pbuf = buf;
            fds[1].events = POLLOUT;
            while (size && do_poll(&fds[1], 1, 0) >= 0) {
                if (fds[1].revents & (POLLERR | POLLHUP))
                    break;
                if (!(fds[1].revents & POLLOUT))
                    continue;
                if ((sent = do_send(fds[1].fd, pbuf, size, 0)) < 0)
                    break;

                pbuf += sent;
                size -= sent;
            }
            if (size)
                break;
        } else if (fds[1].revents & POLLIN) {
            size = do_recv(fds[1].fd, buf, sizeof(buf), 0);
            if (size < 0)
                break;

            pbuf = buf;
            fds[0].events = POLLOUT;
            while (size && do_poll(&fds[0], 1, 0) >= 0) {
                if (fds[0].revents & (POLLERR | POLLHUP))
                    break;

                if (!(fds[0].revents & POLLOUT))
                    continue;
                if ((sent = do_send(fds[0].fd, pbuf, size, 0)) < 0)
                    break;

                pbuf += sent;
                size -= sent;
            }
            if (size)
                break;
        }

        fds[0].events = POLLIN;
        fds[1].events = POLLIN;
    }

    vnet_free_fwd_state(s, 0);
    return NULL;
}

static void vnet_socket_connect(void *opaque)
{
    pthread_t tid;
    PortFwdState *s = opaque;
    printf("vnet_socket_connect\n");

    pthread_create(&tid, NULL, redirect_func, s);
    
    vmx_set_fd_handler(s->redir_fd, NULL, NULL, s);
    vmx_set_fd_handler(s->client_fd, NULL, NULL, s);
    //vmx_set_fd_handler(s->redir_fd, vnet_redir_read, NULL, s);
    //vmx_set_fd_handler(s->client_fd, vnet_client_read, NULL, s);
    
}


static int vnet_init_guest_socket(PortFwdState *s)
{
    // connect to guest socket
    struct sockaddr_in daddr;

    if (!s->port_fwd.guest_addr.s_addr) {
        if (!vm_ip_address)
            return -1;
        s->port_fwd.guest_addr.s_addr = vm_ip_address;
    }
    
    daddr.sin_family = AF_INET;
    memcpy(&daddr.sin_addr.s_addr, &s->port_fwd.guest_addr, sizeof(daddr.sin_addr.s_addr));
    daddr.sin_port = htons(s->port_fwd.guest_port);
    
    s->redir_fd = vmx_socket(PF_INET, SOCK_STREAM, 0);
    if (s->redir_fd < 0) {
        perror("socket");
        return -1;
    }
    
    vmx_set_nonblock(s->redir_fd);

    s->redir_fd_connected = 0;
    for(;;) {
        int ret = connect(s->redir_fd, (struct sockaddr *)&daddr, sizeof(daddr));
        if (ret < 0) {
            int err = socket_error();
            if (err == EINTR || err == EWOULDBLOCK) {
            } else if (err == EINPROGRESS) {
                break;
            } else {
                vnet_free_fwd_state(s, 0);
                return -1;
            }
        } else {
            s->redir_fd_connected = 1;
            break;
        }
    }
    
    vmx_set_fd_handler(s->redir_fd, NULL, vnet_socket_connect, s);
    return 0;
}

static void vnet_socket_accept(void *opaque)
{
    PortFwdState *s = opaque;
    struct sockaddr_in saddr;
    socklen_t len;

    for(;;) {
        len = sizeof(saddr);
        s->client_fd = vmx_accept(s->listen_fd, (struct sockaddr *)&saddr, &len);
        if (s->client_fd < 0 && errno != EINTR) {
            vnet_free_fwd_state(s, 1);
        } else if (s->client_fd  >= 0) {
            vmx_set_nonblock(s->client_fd);
            break;
        }
    }

    if (s->client_fd >= 0 && vnet_init_guest_socket(s) < 0) {
        vnet_free_fwd_state(s, 0);
    }
}

int vnet_add_port_fwd(const char *redir_str)
{
    PortFwdState *s = NULL;
    int fd, ret;
    struct sockaddr_in saddr;

    for (int i = 0; i < ARRAY_SIZE(port_fwd_states); i++) {
        if (!port_fwd_states[i].used) {
            s = &port_fwd_states[i];
            break;
        }
    }
    memset(s, 0, sizeof(*s));
    if (!s)
        return -1;

    if (parse_port_fwd(redir_str, &s->port_fwd) < 0) {
        printf("parse port_fwd failed\n");
        return -1;
    }
    saddr.sin_family = AF_INET;
    memcpy(&saddr.sin_addr.s_addr, &s->port_fwd.host_addr, sizeof(saddr.sin_addr.s_addr));
    saddr.sin_port = htons(s->port_fwd.host_port);

    printf("parse port_fwd\n");
    
    s->listen_fd = vmx_socket(PF_INET, SOCK_STREAM, 0);
    if (s->listen_fd < 0) {
        perror("socket");
        return -1;
    }
    vmx_set_nonblock(s->listen_fd);
    
    socket_set_fast_reuse(s->listen_fd);

    ret = bind(s->listen_fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) {
        perror("bind");
        closesocket(fd);
        return -1;
    }
    ret = listen(s->listen_fd, 0);
    if (ret < 0) {
        perror("listen");
        closesocket(s->listen_fd);
        return -1;
    }

    s->redir_fd = -1;
    s->used = 1;
    
    vmx_set_fd_handler(s->listen_fd, vnet_socket_accept, NULL, s);
    return 0;
}

int vnet_del_port_fwd(const char *redir_str)
{
    PortFwd fwd;

    if (parse_port_fwd(redir_str, &fwd) < 0) {
        printf("parse port_fwd failed\n");
        return -1;
    }
    for (int i = 0; i < ARRAY_SIZE(port_fwd_states); i++) {
        if (port_fwd_states[i].used &&
            port_fwd_states[i].port_fwd.guest_addr.s_addr == fwd.guest_addr.s_addr &&
            port_fwd_states[i].port_fwd.guest_port == fwd.guest_port &&
            port_fwd_states[i].port_fwd.host_addr.s_addr == fwd.host_addr.s_addr &&
            port_fwd_states[i].port_fwd.host_port == fwd.host_port) {

            vnet_free_fwd_state(&port_fwd_states[i], 1);
        }
    }
    return 0;
}
