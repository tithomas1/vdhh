#include "qemu-common.h"
#include "net/checksum.h"
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#define PROTO_TCP  6
#define PROTO_UDP 17

uint32_t ip_checksum_add(uint32_t current, const void *data, int len)
{
    uint32_t sum = current;
    int i;

    const uint8_t *buf = data;
    for (i = 0; i < len - 1; i += 2) {
        sum += htons(*(uint16_t*)(buf + i));
    }

    if (len & 1)
        sum += htons(buf[i]);
    return sum;
}

uint16_t ip_checksum_fold(uint32_t temp_sum)
{
    while(temp_sum > 0xffff)
        temp_sum = (temp_sum >> 16) + (temp_sum & 0xFFFF);
    return temp_sum;
}

uint16_t ip_checksum_finish(uint32_t sum)
{
   return ~ip_checksum_fold(sum);
}

uint16_t ip_checksum(const void *data, int len)
{
    uint32_t temp_sum;
    temp_sum = ip_checksum_add(0, data, len);
    return ip_checksum_finish(temp_sum);
}

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf)
{
    uint32_t sum = 0;

    sum = ip_checksum_add(sum, buf, length);         // payload
    sum = ip_checksum_add(sum, addrs, 8);            // src + dst address
    sum += proto + length;                   // protocol & length
    return ip_checksum_finish(sum);
}

#define ETH_ALEN    6
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/

struct ethhdr {
    unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
    unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
    unsigned short	h_proto;		/* packet type ID field	*/
} __attribute__((packed));

void net_checksum_calculate(uint8_t *data, int length)
{
    int hlen, plen;
    
    struct ethhdr *eth = (struct ethhdr *)data;
    if (htons(eth->h_proto) != ETH_P_IP)
        return;
    struct ip *ip = (struct ip *)(eth + 1);

    hlen  = ip->ip_hl << 2;
    plen  = ntohs(ip->ip_len) - hlen;

    switch (ip->ip_p) {
        case PROTO_TCP: {
            struct tcphdr *tcp = (struct tcphdr *)((uint8_t*)ip + hlen);
            tcp->th_sum = 0;
            tcp->th_sum = ntohs(net_checksum_tcpudp(plen, ip->ip_p, (uint8_t *)&ip->ip_src, (uint8_t *)tcp));
            return;
        }
        case PROTO_UDP: {
            struct udphdr *udp = (struct udphdr *)((uint8_t*)ip + hlen);
            udp->uh_sum = 0;
            udp->uh_sum = ntohs(net_checksum_tcpudp(plen, ip->ip_p, (uint8_t *)&ip->ip_src, (uint8_t *)udp));
            break;
        }
        default:
            break;
    }
}