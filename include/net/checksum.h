#ifndef QEMU_NET_CHECKSUM_H
#define QEMU_NET_CHECKSUM_H

#include <stdint.h>
struct iovec;

uint32_t ip_checksum_add(uint32_t current, const void *data, int len);
uint16_t ip_checksum_finish(uint32_t sum);
uint16_t ip_checksum(const void *data, int len);

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf);
void net_checksum_calculate(uint8_t *data, int length);


/**
 * net_checksum_add_iov: scatter-gather vector checksumming
 *
 * @iov: input scatter-gather array
 * @iov_cnt: number of array elements
 * @iov_off: starting iov offset for checksumming
 * @size: length of data to be checksummed
 */
uint32_t net_checksum_add_iov(const struct iovec *iov,
                              const unsigned int iov_cnt,
                              uint32_t iov_off, uint32_t size);

#endif /* QEMU_NET_CHECKSUM_H */
