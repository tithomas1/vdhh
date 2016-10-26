#ifndef __ETH_H__
#define __ETH_H__

static inline int is_multicast_ether_addr(const uint8_t *addr)
{
    return 0x01 & addr[0];
}

static inline int is_broadcast_ether_addr(const uint8_t *addr)
{
    return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

static inline int is_unicast_ether_addr(const uint8_t *addr)
{
    return !is_multicast_ether_addr(addr);
}

#endif