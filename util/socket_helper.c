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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "monitor/monitor.h"
#include "qemu/sockets.h"

/* used temporarily until all users are converted to QemuOpts */
QemuOptsList socket_optslist = {
    .name = "socket",
    .head = QTAILQ_HEAD_INITIALIZER(socket_optslist.head),
    .desc = {
        {
            .name = "path",
            .type = QEMU_OPT_STRING,
        },{
            .name = "host",
            .type = QEMU_OPT_STRING,
        },{
            .name = "port",
            .type = QEMU_OPT_STRING,
        },{
            .name = "ipv4",
            .type = QEMU_OPT_BOOL,
        },
        { /* end if list */ }
    },
};

int unix_listen_opts(QemuOpts *opts, Error **errp)
{
    struct sockaddr_un un;
    const char *path = vmx_opt_get(opts, "path");
    int sock, fd;
    
    sock = vmx_socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        error_setg_errno(errp, errno, "Failed to create socket");
        return -1;
    }
    
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    if (path && strlen(path)) {
        snprintf(un.sun_path, sizeof(un.sun_path), "%s", path);
    } else {
        char *tmpdir = getenv("TMPDIR");
        snprintf(un.sun_path, sizeof(un.sun_path), "%s/vmx-socket-XXXXXX",
                 tmpdir ? tmpdir : "/tmp");
        /*
         * This dummy fd usage silences the mktemp() unsecure warning.
         * Using mkstemp() doesn't make things more secure here
         * though.  bind() complains about existing files, so we have
         * to unlink first and thus re-open the race window.  The
         * worst case possible is bind() failing, i.e. a DoS attack.
         */
        fd = mkstemp(un.sun_path); close(fd);
        vmx_opt_set(opts, "path", un.sun_path);
    }
    
    unlink(un.sun_path);
    if (bind(sock, (struct sockaddr*) &un, sizeof(un)) < 0) {
        error_setg_errno(errp, errno, "Failed to bind socket");
        goto err;
    }
    if (listen(sock, 1) < 0) {
        error_setg_errno(errp, errno, "Failed to listen on socket");
        goto err;
    }
    
    return sock;
err:
    closesocket(sock);
    return -1;
}

int socket_listen(SocketAddress *addr, Error **errp)
{
    QemuOpts *opts;
    int fd;
    
    opts = vmx_opts_create(&socket_optslist, NULL, 0, &error_abort);
    switch (addr->kind) {
        case SOCKET_ADDRESS_KIND_INET:
            error_setg_errno(errp, errno, "Implement tcp socket");
            fd = -1;
            break;
            
        case SOCKET_ADDRESS_KIND_UNIX:
            vmx_opt_set(opts, "path", addr->q_unix->path);
            fd = unix_listen_opts(opts, errp);
            break;
            
        case SOCKET_ADDRESS_KIND_FD:
            error_setg_errno(errp, errno, "imelement fd socket");
            fd = -1;
            break;
            
        default:
            abort();
    }
    vmx_opts_del(opts);
    return fd;
}
