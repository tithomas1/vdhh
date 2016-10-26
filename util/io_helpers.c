//
//  io_helpers.c
//  Veertu VMX
//
/*
 * Simple C functions to supplement the C library
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "io_helpers.h"
#include "qemu/sockets.h"

size_t iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
                    size_t offset, const void *buf, size_t bytes)
{
    int total = 0;

    while (bytes > 0 && iov_cnt) {
        size_t len = MIN(bytes, iov->iov_len);
        memcpy(iov->iov_base, buf, len);

        total += len;
        bytes -= len;
        buf += len;

        iov++;
        iov_cnt--;
    }
    return total;
}

size_t iov_to_buf(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, void *buf, size_t bytes)
{
    int total = 0;
    for (int i = 0; i < iov_cnt && bytes; i++) {
        size_t len = MIN(bytes, iov->iov_len);
        memcpy(buf, iov->iov_base, len);
        total += len;
        buf += len;
        bytes -= len;
        iov++;
    }
    return total;
}

size_t iov_memset(const struct iovec *iov, unsigned int iov_cnt,
                  size_t offset, int fillc, size_t bytes)
{
    size_t set = 0;

    for (int i = 0; (offset || set < bytes) && i < iov_cnt; i++) {
        if (offset >= iov[i].iov_len) {
            offset -= iov[i].iov_len;
            continue;
        }

        size_t len = MIN(iov[i].iov_len - offset, bytes - set);
        memset(iov[i].iov_base + offset, fillc, len);
        set += len;
        offset = 0;
    }
    assert(offset == 0);
    return set;
}

ssize_t iov_send_recv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                      size_t offset, size_t bytes,
                      bool do_send)
{
    int ret, diff, iovlen;
    struct iovec *last_iov;
    
    /* last_iov is inclusive, so count from one.  */
    iovlen = 1;
    last_iov = iov;
    bytes += offset;
    
    while (last_iov->iov_len < bytes) {
        bytes -= last_iov->iov_len;
        
        last_iov++;
        iovlen++;
    }
    
    diff = last_iov->iov_len - bytes;
    last_iov->iov_len -= diff;
    
    while (iov->iov_len <= offset) {
        offset -= iov->iov_len;
        
        iov++;
        iovlen--;
    }
    
    iov->iov_base = (char *) iov->iov_base + offset;
    iov->iov_len -= offset;
    
    {
        struct iovec *p = iov;
        ret = 0;
        while (iovlen > 0) {
            int rc;
            if (do_send) {
                rc = send(sockfd, p->iov_base, p->iov_len, 0);
            } else {
                rc = recv(sockfd, p->iov_base, p->iov_len, 0);
            }
            if (rc == -1) {
                if (errno == EINTR) {
                    continue;
                }
                if (ret == 0) {
                    ret = -1;
                }
                break;
            }
            if (rc == 0) {
                break;
            }
            ret += rc;
            iovlen--, p++;
        }
    }
    
    /* Undo the changes above */
    iov->iov_base = (char *) iov->iov_base - offset;
    iov->iov_len += offset;
    last_iov->iov_len += diff;
    return ret;
}

size_t iov_discard_back(struct iovec *iov, unsigned int *iov_cnt, size_t bytes)
{
    size_t total = 0;
    
    if (!*iov_cnt)
        return 0;

    iov += *iov_cnt - 1;

    while (*iov_cnt > 0 && bytes > 0) {
        size_t len = MIN(iov->iov_len, bytes);
        
        iov->iov_len -= len;
        total += len;

        bytes -= len;
        total += len;
        iov--;
        *iov_cnt -= 1;
    }

    return total;
}

void vmx_iovec_init(QEMUIOVector *qiov, int alloc_hint)
{
    qiov->iov = g_new(struct iovec, alloc_hint);
    qiov->niov = 0;
    qiov->nalloc = alloc_hint;
    qiov->size = 0;
}

void vmx_iovec_init_external(QEMUIOVector *qiov, struct iovec *iov, int niov)
{
    qiov->iov = iov;
    qiov->niov = niov;
    qiov->nalloc = -1;
    qiov->size = 0;
    for (int i = 0; i < niov; i++)
        qiov->size += iov[i].iov_len;
}

void vmx_iovec_add(QEMUIOVector *qiov, void *base, size_t len)
{
    assert(qiov->nalloc != -1);
    
    if (qiov->niov == qiov->nalloc) {
        qiov->nalloc = 2 * qiov->nalloc + 1;
        qiov->iov = g_renew(struct iovec, qiov->iov, qiov->nalloc);
    }
    qiov->iov[qiov->niov].iov_base = base;
    qiov->iov[qiov->niov].iov_len = len;
    qiov->size += len;
    ++qiov->niov;
}

size_t vmx_iovec_to_buf(QEMUIOVector *qiov, size_t offset,
                         void *buf, size_t bytes)
{
    return iov_to_buf(qiov->iov, qiov->niov, offset, buf, bytes);
}

size_t vmx_iovec_from_buf(QEMUIOVector *qiov, size_t offset,
                           const void *buf, size_t bytes)
{
    return iov_from_buf(qiov->iov, qiov->niov, offset, buf, bytes);
}

size_t vmx_iovec_memset(QEMUIOVector *qiov, size_t offset,
                         int fillc, size_t bytes)
{
    return iov_memset(qiov->iov, qiov->niov, offset, fillc, bytes);
}

void vmx_iovec_discard_back(QEMUIOVector *qiov, size_t bytes)
{
    unsigned int niov = qiov->niov;
    iov_discard_back(qiov->iov, &niov, bytes);

    qiov->niov = niov;
    qiov->size -= bytes;
}

void vmx_iovec_reset(QEMUIOVector *qiov)
{
    qiov->niov = 0;
    qiov->size = 0;
}

void vmx_iovec_destroy(QEMUIOVector *qiov)
{
    vmx_iovec_reset(qiov);
    g_free(qiov->iov);
    qiov->nalloc = 0;
    qiov->iov = NULL;
}

static bool is_buf_empty(char *buf, size_t size)
{
    return buf[0] == 0 && !memcmp(buf, buf + 1, size - 1);
}

bool vmx_iovec_is_zero(QEMUIOVector *qiov)
{
    for (int i = 0; i < qiov->niov; i++) {
        if (!is_buf_empty(qiov->iov[i].iov_base, qiov->iov[i].iov_len))
            return false;
    }
    return true;
}

/*
 * Copies iovecs from src to the end of dst. It starts copying after skipping
 * the given number of bytes in src and copies until src is completely copied
 * or the total size of the copied iovec reaches size.The size of the last
 * copied iovec is changed in order to fit the specified total size if it isn't
 * a perfect fit already.
 */
void vmx_iovec_copy(QEMUIOVector *dst, QEMUIOVector *src, uint64_t skip, size_t size)
{
    int i;
    size_t done;
    void *iov_base;
    uint64_t iov_len;
    
    assert(dst->nalloc != -1);
    
    done = 0;
    for (i = 0; (i < src->niov) && (done != size); i++) {
        if (skip >= src->iov[i].iov_len) {
            /* Skip the whole iov */
            skip -= src->iov[i].iov_len;
            continue;
        } else {
            /* Skip only part (or nothing) of the iov */
            iov_base = (uint8_t*) src->iov[i].iov_base + skip;
            iov_len = src->iov[i].iov_len - skip;
            skip = 0;
        }
        
        if (done + iov_len > size) {
            vmx_iovec_add(dst, iov_base, size - done);
            break;
        } else {
            vmx_iovec_add(dst, iov_base, iov_len);
        }
        done += iov_len;
    }
}

void vmx_iovec_concat(QEMUIOVector *dst, QEMUIOVector *src, size_t soffset, size_t sbytes)
{
    vmx_iovec_copy(dst, src, soffset, sbytes);
}

