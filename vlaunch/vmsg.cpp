#include "vmsg.h"
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cassert>
#include <vector>
#include <mutex>
#include <map>


namespace {

// serialization
typedef struct vmsg_hdr {
    uint32_t length;
} __attribute__((packed)) vmsg_hdr_t;

typedef struct vmsg_read_context {
    std::vector<uint8_t> buffer;
    size_t offset;
} vmsg_read_context_t;

#ifndef RECV_BUF_SIZE
static const size_t RECV_BUF_SIZE = 128;    // must be as large as to be able
                                            // to operate on DGRAM and character devices atomically
#endif

// global cache of all file descriptors and associated receive buffers
std::map<int, vmsg_read_context_t> commands;
std::mutex lock;

inline vmsg_read_context_t& get_context(int fd) {
    std::lock_guard<std::mutex> sync(lock);
    return commands[fd];
}

inline void del_context(int fd) {
    std::lock_guard<std::mutex> sync(lock);
    commands.erase(fd);
}

int vmsg_read_internal(int fd, vobj_t msg, vmsg_read_context_t& ctx) {
    if (ctx.buffer.size() >= ctx.offset + sizeof(vmsg_hdr_t)) {
        const vmsg_hdr_t* hdr = reinterpret_cast<const vmsg_hdr_t*>(&ctx.buffer.front() + ctx.offset);
        size_t length = htonl(hdr->length);
        if (ctx.offset + (length + sizeof(*hdr)) <= ctx.buffer.size()) {
            ctx.offset += length + sizeof(*hdr);
            // have full msg in the receive buffer, return it
            if (-1 == vobj_set_data(msg, hdr + 1, length)) {
                errno = EBADMSG;
                return -1;
            }
            return length;
        }
    }

    // we have no full msg into the buffer, read next portion from fd
    if (ctx.offset > 0) {
        // drop all previous messages from buffer
        std::vector<uint8_t>(ctx.buffer.begin() + ctx.offset, ctx.buffer.end()).swap(ctx.buffer);
        ctx.offset = 0;
    }

    std::unique_ptr<uint8_t[]> rcvbuf(new uint8_t[RECV_BUF_SIZE]);
    ssize_t len = read(fd, rcvbuf.get(), RECV_BUF_SIZE);
    if (len <= 0) {
        // fd looks closed or failed
        del_context(fd);
        return len;
    }

    ctx.buffer.insert(ctx.buffer.end(), rcvbuf.get(), rcvbuf.get() + len);
    return vmsg_read_internal(fd, msg, ctx);
}

} // namespace

extern "C" {

int vmsg_read(int fd, vobj_t msg) {
    return vmsg_read_internal(fd, msg, get_context(fd));
}

int vmsg_write(int fd, vobj_t msg) {
    // serialize the msg
    std::vector<uint8_t> buffer(RECV_BUF_SIZE);
    size_t size = buffer.size();
    int res = vobj_get_data(msg, &buffer.front(), &size);
    if (-1 == res && ENOMEM == errno) {
        // retry with larger buffer
        buffer.resize(size);
        res = vobj_get_data(msg, &buffer.front(), &size);
    }

    assert(0 == res);
    if (res < 0)
        return res;

    // send
    vmsg_hdr hdr = {0};
    hdr.length = htonl(size);

    // prepare respond chunk as atomic write to fd
    iovec io[] = {
        {&hdr, sizeof(hdr)},
        {&buffer.front(), size}
    };

    // blow to file descriptor
    int w = writev(fd, io, sizeof(io) / sizeof(*io));
    return w == size + sizeof(hdr) ? size : w;
}

} // extern "C"
