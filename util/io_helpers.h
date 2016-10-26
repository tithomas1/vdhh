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

#ifndef __IO_HELPERS_H__
#define __IO_HELPERS_H__

#include "qemu-common.h"

static inline size_t iov_size(const struct iovec *iov, const unsigned int iov_cnt)
{
    size_t len = 0;
    for (int i = 0; i < iov_cnt; i++)
        len += iov[i].iov_len;
    return len;
}

size_t iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
                    size_t offset, const void *buf, size_t bytes);

size_t iov_to_buf(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, void *buf, size_t bytes);

size_t iov_memset(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, int fillc, size_t bytes);

ssize_t iov_send_recv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                      size_t offset, size_t bytes, bool do_send);

static inline ssize_t iov_recv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                               size_t offset, size_t bytes)
{
    return iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, false);
}

static inline ssize_t  iov_send(int sockfd, struct iovec *iov, unsigned iov_cnt,
                                size_t offset, size_t bytes)
{
    return iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, true);
}

#endif /* io_helpers_h */
