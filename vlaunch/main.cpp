#include "vlaunch.h"
#include "log.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <spawn.h>
#include <unistd.h>
#include <vector>
#include <memory>
#include <cassert>


#define VLAUNCH_DEF_SOCKET_PATH "/var/tmp/com.veertu.vlaunch.socket"

int create_fd(const char* hint) {

    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (-1 == fd) {
        return -1;
    }

    sockaddr_un addr = {
        sizeof(sockaddr_un),
        AF_LOCAL,
        VLAUNCH_DEF_SOCKET_PATH
    };
    unlink(addr.sun_path);
    if (-1 == bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        close(fd);
        return -1;
    }

    if (0 == geteuid())
        chmod(addr.sun_path, 0666);

    if (-1 == listen(fd, 2)) {
        close(fd);
        return -1;
    }

    int flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag));

    return fd;
}

int main (int argc, const char* argv[]) {

    int fd = create_fd(argc > 1 ? argv[1] : NULL);
    if (-1 == fd)
        return errno;

    fd_set rdset;
    std::vector<int> clients;
    do {
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);

        // set all connected clients too
        int maxfd = fd;
        for (auto cfd : clients) {
            FD_SET(cfd, &rdset);
            maxfd = std::max(maxfd, cfd);
        }

        // wait for incoming data
        if (-1 == select(maxfd + 1, &rdset, NULL, NULL, NULL)) {
            if (EAGAIN == errno || EINTR == errno)
                continue;
            LOG("Error %d while selecting. Exiting gracefully", errno);
            break;
        }

        // process requests from connections
        std::vector<int> activefd;
        for (auto cfd : clients) {
            if (FD_ISSET(cfd, &rdset)) {
                int res = vlaunch_run_once(cfd, cfd);
                if (res <= 0) {
                    // connection closed or failed
                    close(cfd);
                    LOG("Connection to %d closed", cfd);
                    continue;
                }
            }
            activefd.push_back(cfd);
        }
        clients.swap(activefd);

        // process incoming connections
        if (FD_ISSET(fd, &rdset)) {
            sockaddr_un addr = {0};
            socklen_t len = sizeof(addr);
            int cfd = accept(fd, reinterpret_cast<sockaddr*>(&addr), &len);
            if (-1 == cfd) {
                LOG("Incoming connection acceptance failed");
                continue;
            }

            // TODO: check client's address

            int flag = 1;
            setsockopt(cfd, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag));

            LOG("Accepted new client %d", cfd);
            clients.push_back(cfd);
        }
    } while(true);

    LOG("Closing service socket");
    close(fd);
    fd = -1;
    unlink(argc > 1 ? argv[1] : VLAUNCH_DEF_SOCKET_PATH);

    // close all connections gracefully
    LOG("Closing client connections");
    for (auto cfd : clients) {
        shutdown(cfd, SHUT_RDWR);
        close(cfd);
    }
    clients.clear();

    return 0;
}
