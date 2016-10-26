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
#include <sys/un.h>
#include <net/if.h>

#include "net/net.h"
#include "clients.h"
#include "monitor/monitor.h"
#include "sysemu.h"
#include "qemu-common.h"
#include "qemu/error-report.h"

#include "net/tap.h"
#include <vmnet/vmnet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include "window/cocoa_util.h"
#include "vnet_fwd.h"

#include <vlaunch/vmsg.h>
#include <vlaunch/vobj.h>


extern uint32_t vm_ip_address;

struct arphdr {
    unsigned short ar_hrd;      /* format of hardware address */
    unsigned short ar_pro;      /* format of protocol address */
    unsigned char  ar_hln;      /* length of hardware address */
    unsigned char  ar_pln;      /* length of protocol address */
    unsigned short ar_op;       /* ARP opcode (command)       */
    
    /*
     *  Ethernet looks like this : This bit is variable sized however...
     */
    unsigned char ar_sha[ETHER_ADDR_LEN]; /* sender hardware address */
    uint32_t      ar_sip;           /* sender IP address       */
    unsigned char ar_tha[ETHER_ADDR_LEN]; /* target hardware address */
    uint32_t      ar_tip;           /* target IP address       */
};

typedef struct VnetState {
    NetClientState nc;
    interface_ref iface;
    bool enabled;
    uint8_t hw_mac[6];
    uint8_t vnet_mac[6];
    bool hw_mac_set;
    bool read_poll;
    bool write_poll;
    int fd[2];
    int event_cnt;
    uint32_t ipaddr;
    uint8_t buf_snd[2048];
    uint8_t buf_rcv[2048];
    int proxyfd;
} VnetState;

void vnet_flush(VnetState *s)
{
    struct vmpktdesc pkt_desc;
    int pkt_cnt = 1;
    struct iovec iov;
    uint8_t pkt_buf[2048];

    if (s->iface) {
        iov.iov_base = pkt_buf;
        iov.iov_len = sizeof(pkt_buf);

        pkt_desc.vm_flags = 0;
        pkt_desc.vm_pkt_iov = &iov;
        pkt_desc.vm_pkt_iovcnt = 1;
        pkt_desc.vm_pkt_size = sizeof(pkt_buf);
        
        vmnet_read(s->iface, &pkt_desc, &pkt_cnt);
    }

    if (-1 != s->proxyfd) {
        read(s->proxyfd, pkt_buf, sizeof(pkt_buf));
    }
}

dispatch_queue_t queue = NULL;

static int vnet_can_send(void *opaque)
{
    VnetState *s = opaque;

    bool can_send = vmx_can_send_packet(&s->nc);
    if (!can_send) {
        // dummy read to clear the buffer
        vnet_flush(s);
    }
    return can_send;
}

static void vnet_send(void *opaque);
static void vnet_writable(void *opaque);

static void vnet_update_fd_handler(VnetState *s)
{
    vmx_set_fd_handler2(s->iface ? s->fd[0] : s->proxyfd,
                         s->read_poll && s->enabled ? vnet_can_send : NULL,
                         s->read_poll && s->enabled ? vnet_send     : NULL,
                         s->write_poll && s->enabled ? vnet_writable : NULL,
                         s);
}

static void vnet_read_poll(VnetState *s, bool enable)
{
    s->read_poll = enable;
    vnet_update_fd_handler(s);
}

static void vnet_write_poll(VnetState *s, bool enable)
{
    s->write_poll = enable;
    vnet_update_fd_handler(s);
}

static void vnet_writable(void *opaque)
{
    VnetState *s = opaque;
    
    vnet_write_poll(s, false);
    
    vmx_flush_queued_packets(&s->nc);
}

static void vnet_send_completed(NetClientState *nc, ssize_t len)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    vnet_read_poll(s, true);
}

int iov_tot_len(const struct iovec *iov, int iovcnt)
{
    int len = 0;
    int i;
    
    for (i = 0; i < iovcnt; i++)
        len += iov[i].iov_len;
    return len;
}

#define UDP_PORT_BOOTPC_N 0x4400
#define UDP_PORT_BOOTPS_N 0x4300


static void vnet_mac_change(VnetState *s, uint8_t *pkt, size_t pkt_len, bool send_to_vm)
{
    struct ether_header *eth = (struct ether_header *)pkt;
    if (send_to_vm && !memcmp(eth->ether_dhost, s->vnet_mac, ETHER_ADDR_LEN)) {
        memcpy(eth->ether_dhost, s->hw_mac, ETHER_ADDR_LEN);
    } else if (!send_to_vm && !memcmp(eth->ether_shost, s->hw_mac, ETHER_ADDR_LEN)) {
        memcpy(eth->ether_shost, s->vnet_mac, ETHER_ADDR_LEN);
    }
}

static void vnet_mac_change_for_arp(VnetState *s, uint8_t *pkt, size_t pkt_len, bool send_to_vm)
{
    struct ether_header *eth = (struct ether_header *)pkt;

    if (send_to_vm && ntohs(eth->ether_type) == ETHERTYPE_ARP) {
        struct arphdr *arp = (struct arphdr*)(eth + 1);
        if (!memcmp(arp->ar_tha, s->vnet_mac, ETHER_ADDR_LEN)) {
            memcpy(arp->ar_tha, s->hw_mac, ETHER_ADDR_LEN);
        }
    }
    else if (!send_to_vm && ntohs(eth->ether_type) == ETHERTYPE_ARP) {
        struct arphdr *arp = (struct arphdr*)(eth + 1);
        if (!memcmp(arp->ar_sha, s->hw_mac, ETHER_ADDR_LEN)) {
            memcpy(arp->ar_sha, s->vnet_mac, ETHER_ADDR_LEN);
        }
    }
}

static void vnet_mac_change_for_dhcp(VnetState *s, uint8_t *pkt, size_t pkt_len, bool send_to_vm)
{
    struct ether_header *eth = (struct ether_header *)pkt;

    if (ntohs(eth->ether_type) == ETHERTYPE_IP) {
        struct ip* ip = (struct ip*)(eth + 1);
        if (ip->ip_p == IPPROTO_UDP) {
            struct udphdr *udp = (struct udphdr*)(((uint8_t*)ip) + (ip->ip_hl * sizeof(uint32_t)));
            struct bootp *bootp = (struct bootp *)(udp+1);

            if (send_to_vm && UDP_PORT_BOOTPC_N == udp->uh_dport &&
                UDP_PORT_BOOTPS_N == udp->uh_sport) {

                if (1 == bootp->bp_htype && 6 == bootp->bp_hlen &&
                    !memcmp(bootp->bp_chaddr, s->vnet_mac, ETHER_ADDR_LEN)) {
                    memcpy(bootp->bp_chaddr, s->hw_mac, ETHER_ADDR_LEN);
                    net_checksum_calculate(pkt, pkt_len);

                    s->ipaddr = bootp->bp_yiaddr.s_addr;
                    vm_ip_address = s->ipaddr;
                }
            }
            else if (!send_to_vm && UDP_PORT_BOOTPS_N == udp->uh_dport &&
                     UDP_PORT_BOOTPC_N == udp->uh_sport) {

                if (1 == bootp->bp_htype && 6 == bootp->bp_hlen &&
                    !memcmp(bootp->bp_chaddr, s->hw_mac, ETHER_ADDR_LEN)) {
                    memcpy(bootp->bp_chaddr, s->vnet_mac, ETHER_ADDR_LEN);
                    net_checksum_calculate(pkt, pkt_len);
                }
            }
        }
    }
}

ssize_t vnet_write_packet(VnetState *s, const struct iovec *iov, int iovcnt)
{
    struct vmpktdesc pkt_desc;
    int pkt_cnt = 1;

    if (iovcnt > 1) {
        int i;
        size_t copied = 0;
        struct iovec new_iov;

        if (iov_tot_len(iov, iovcnt) > sizeof(s->buf_snd)) {
            NSLog("packet len %d too large for the buffer", iov_tot_len(iov, iovcnt));
            return -1;
        }
        for (i = 0; i < iovcnt; i++) {
            memcpy(s->buf_snd + copied, iov[i].iov_base, iov[i].iov_len);
            copied += iov[i].iov_len;
        }

        new_iov.iov_base = s->buf_snd;
        new_iov.iov_len = copied;
        pkt_desc.vm_pkt_size = copied;
        pkt_desc.vm_pkt_iov = &new_iov;
        pkt_desc.vm_pkt_iovcnt = 1;
        pkt_desc.vm_flags = 0;
    } else {
        pkt_desc.vm_pkt_size = iov_tot_len(iov, iovcnt);
        pkt_desc.vm_pkt_iov = iov;
        pkt_desc.vm_pkt_iovcnt = iovcnt;
        pkt_desc.vm_flags = 0;
    }

    vnet_mac_change(s, pkt_desc.vm_pkt_iov->iov_base, pkt_desc.vm_pkt_size, false);
    vnet_mac_change_for_arp(s, pkt_desc.vm_pkt_iov->iov_base, pkt_desc.vm_pkt_size, false);
    vnet_mac_change_for_dhcp(s, pkt_desc.vm_pkt_iov->iov_base, pkt_desc.vm_pkt_size, false);

    if (s->iface) {
        vmnet_write(s->iface, &pkt_desc, &pkt_cnt);
        return pkt_desc.vm_pkt_size;
    }

    if (-1 != s->proxyfd) {
        return writev(s->proxyfd, iov, iovcnt);
    }

    assert(false);
    return 0;
}

static ssize_t vnet_receive_iov(NetClientState *nc, const struct iovec *iov, int iovcnt)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    return vnet_write_packet(s, iov, iovcnt);
}

static ssize_t vnet_receive_raw(NetClientState *nc, const uint8_t *buf, size_t size)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    struct iovec iov[1];

    iov[0].iov_base = (char *)buf;
    iov[0].iov_len  = size;
    
    return vnet_write_packet(s, iov, 1);
}

static ssize_t vnet_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    struct iovec iov[1];
    iov[0].iov_base = (char *)buf;
    iov[0].iov_len  = size;
    return vnet_write_packet(s, iov, 1);
}

static void vnet_cleanup(NetClientState *nc)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    
    vmx_purge_queued_packets(nc);

    if (s->iface) {
        vmnet_stop_interface(s->iface, queue,
            ^(vmnet_return_t status)
            {
            }
        );
        s->iface = NULL;
    }

    if (-1 != s->proxyfd) {
        // send terminating packet for UDP client
        uint32_t term = htonl(0x999);
        write(s->proxyfd, &term, sizeof(term));

        struct sockaddr_un local = {0};
        socklen_t addrlen = sizeof(local);
        getsockname(s->proxyfd, (struct sockaddr*)&local, &addrlen);

        close(s->proxyfd);
        s->proxyfd = -1;

        if (local.sun_path[0])
            unlink(local.sun_path);
    }
}

static void vnet_poll(NetClientState *nc, bool enable)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    vnet_read_poll(s, enable);
    vnet_write_poll(s, enable);
}

static NetClientInfo net_vnet_info = {
    .type = NET_CLIENT_OPTIONS_KIND_VNET,
    .size = sizeof(VnetState),
    .receive = vnet_receive,
    .receive_raw = vnet_receive_raw,
    .receive_iov = vnet_receive_iov,
    .poll = vnet_poll,
    .cleanup = vnet_cleanup,
};

static void vnet_send(void *opaque)
{
    NetClientState *nc = opaque;
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    int pkt_cnt = 1;
    struct iovec iov;

    iov.iov_base = s->buf_rcv;
    iov.iov_len = sizeof(s->buf_rcv);

    int pktlen = 0;
    if (-1 != s->proxyfd) {
        pktlen = readv(s->proxyfd, &iov, 1);
        if (pktlen <= 0)
            return;
    } else {
        char c;
        while (read(s->fd[0], &c, 1) >= 0);

        // direct call to vmnet
        struct vmpktdesc pkt_desc;
        pkt_desc.vm_flags = 0;
        pkt_desc.vm_pkt_iov = &iov;
        pkt_desc.vm_pkt_iovcnt = 1;
        pkt_desc.vm_pkt_size = sizeof(s->buf_rcv);

        vmnet_return_t res = vmnet_read(s->iface, &pkt_desc, &pkt_cnt);
        if (res != VMNET_SUCCESS)
            return;

        if (pkt_desc.vm_pkt_size == sizeof(s->buf_rcv)) {
            // weird bug: received a dummy buffer
            // drop it as a workaround
            return;
        }
        pktlen = pkt_desc.vm_pkt_size;
    }

    assert(pktlen >= 0 && pktlen < sizeof(s->buf_rcv));
    vnet_mac_change(s, s->buf_rcv, pktlen, true);
    vnet_mac_change_for_arp(s, s->buf_rcv, pktlen, true);
    vnet_mac_change_for_dhcp(s, s->buf_rcv, pktlen, true);

    if (vmx_send_packet_async(nc, s->buf_rcv, pktlen, vnet_send_completed) == 0)
        vnet_read_poll(s, false);
}

static void vnet_save_state(QEMUFile *f, void *opaque)
{
    VnetState *s = (struct VnetState*)opaque;
    vmx_put_be32(f, s->ipaddr);
}

static int vnet_load_state(QEMUFile *f, void *opaque, int version_id)
{
    VnetState *s = (struct VnetState*)opaque;
    s->ipaddr = vmx_get_be32(f);
    vm_ip_address = s->ipaddr;

    return 0;
}


static VnetState *net_vnet_fd_init(NetClientState *peer,
                                 const char *model,
                                 const char *name,
                                 uint8_t *vnet_mac,
                                 interface_ref iface,
                                 int fd)
{
    NetClientState *nc;
    VnetState *s;

    assert(NULL == iface || -1 == fd);

    nc = vmx_new_net_client(&net_vnet_info, peer, model, name);
    
    s = DO_UPCAST(VnetState, nc, nc);

    fcntl(fd, F_SETFL, O_NONBLOCK);
    s->proxyfd = fd;
    s->iface = iface;
    s->enabled = true;
    s->hw_mac_set = false;
    memcpy(s->vnet_mac, vnet_mac, ETHER_ADDR_LEN);
    s->ipaddr = 0;
    vm_ip_address = 0;

    register_savevm(NULL, "vnet", 0, 1, vnet_save_state, vnet_load_state, s);

    if (iface) {
        pipe(s->fd);
        fcntl(s->fd[0], F_SETFL, O_NONBLOCK);
        s->event_cnt = 0;

        vmnet_interface_set_event_callback(s->iface, VMNET_INTERFACE_PACKETS_AVAILABLE, queue,
                                       ^(interface_event_t event_id, xpc_object_t event)
                                       {
                                           int c = 0;
                                           if (!s->event_cnt) {
                                               write(s->fd[1], &c, 1);
                                            }
                                       });
    }

    vnet_read_poll(s, true);
    return s;
}

#define MAX_ATTEMPTS    16

static void uuid_rotate(uuid_t uuid)
{
    uuid[0]++;
    uuid[1]+=2;
    uuid[2]+=3;
}

static interface_ref net_init_vmnet(int mode, uuid_t uuid, uint8_t lmac[6]) {
    bool res = false;
    int attempts = 0;
    __block struct {
        char a[64];
        int mtu;
        int max_pkt_size;
    } param;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // shared queue for events on all interfaces
        queue = dispatch_queue_create("com.veertu.vmnet.create", DISPATCH_QUEUE_SERIAL);
    });

    __block interface_ref iface = NULL;

    xpc_object_t iface_desc = xpc_dictionary_create(NULL, NULL, 0);
    dispatch_semaphore_t iface_created = dispatch_semaphore_create(0);

    xpc_dictionary_set_uint64(iface_desc, vmnet_operation_mode_key, mode);
    xpc_dictionary_set_uuid(iface_desc, vmnet_interface_id_key, uuid);

    iface = vmnet_start_interface(iface_desc, queue,
                                  ^(vmnet_return_t status, xpc_object_t interface_param) {
        if (status != VMNET_SUCCESS || !interface_param) {
            iface = NULL;
            dispatch_semaphore_signal(iface_created);
            return;
        }

        const char *macStr = xpc_dictionary_get_string(interface_param, vmnet_mac_address_key);
        param.mtu = xpc_dictionary_get_uint64(interface_param, vmnet_mtu_key);
        param.max_pkt_size = xpc_dictionary_get_uint64(interface_param, vmnet_max_packet_size_key);

        strncpy(param.a, macStr, sizeof(param.a));
        dispatch_semaphore_signal(iface_created);
    });

    dispatch_semaphore_wait(iface_created, DISPATCH_TIME_FOREVER);

    if (!iface) {
        displayAlert("Failed to create network connection", "Try again later", "Close");
        return NULL;
    }
    
    sscanf(param.a, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &lmac[0], &lmac[1], &lmac[2], &lmac[3], &lmac[4], &lmac[5]);

    return iface;
}

static int create_socket(const char* path) {

    int s = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (-1 == s)
        return -1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (-1 == bind(s, (struct sockaddr*)&addr, sizeof(addr))) {
        close(s);
        return -1;
    }

    chmod(addr.sun_path, 0666);

    return s;
}

static int net_init_proxy(int mode, uuid_t uuid, uint8_t lmac[6]) {
    // create communication socket and pass it to proxy process
    char path[] = "/tmp/com.veertu.vnet.XXXXX";
    mkstemp(path);
    unlink(path);
    int fd = create_socket(path);
    if (-1 == fd) {
        return -1;
    }

    // launch the proxy
    char vcmd[128];
    snprintf(vcmd, sizeof(vcmd), "vmnetproxy %s %s", path, mode == VMNET_HOST_MODE ? "host" : "shared");
    if (!uuid_is_null(uuid)) {
        uuid_string_t buf;
        uuid_unparse(uuid, buf);
        strncat(vcmd, " ", sizeof(vcmd));
        strncat(vcmd, buf, sizeof(vcmd));
    }
    if (0 != vsystem(vcmd, 0)) {
        close(fd);
        unlink(path);
        return -1;
    }

    struct sockaddr_un peer;
    socklen_t addrlen = sizeof(peer);
    int len = recvfrom(fd, lmac, ETHER_ADDR_LEN, 0, (struct sockaddr*)&peer, &addrlen);
    if (len < 6) {
        close(fd);
        return -1;
    }

    if (-1 == connect(fd, (struct sockaddr*)&peer, addrlen)) {
        close(fd);
        return -1;
    }

    struct ether_addr dummy = {0};
    if (len < ETHER_ADDR_LEN || memcmp(lmac, &dummy, ETHER_ADDR_LEN) == 0) {
        close(fd);
        displayAlert("Failed to create network connection", "Try again later", "Close");
        return -1;
    }

    return fd;
}

int net_init_vnet(const NetClientOptions *opts, const char *name, NetClientState *peer)
{
    const NetdevVnetOptions *vnet_opts;
    int mode = VMNET_SHARED_MODE;

    assert(opts->kind == NET_CLIENT_OPTIONS_KIND_VNET);
    vnet_opts = opts->vnet;

    uuid_t uuid = {0};
    uuid_generate_random(uuid);

    if (vnet_opts->has_mode) {
        if (!strcasecmp(vnet_opts->mode, "shared"))
            mode = VMNET_SHARED_MODE;
        else if (!strcasecmp(vnet_opts->mode, "host"))
            mode = VMNET_HOST_MODE;
        else {
            //printf("invalid mode %s, must be 'host' or 'shared'\n", vnet_opts->mode);
            return -1;
        }
    }
    if (vnet_opts->has_uuid) {
        memset(uuid, 0, sizeof(uuid_t));
        memcpy(uuid, vnet_opts->uuid, MIN(sizeof(uuid_t), strlen(vnet_opts->uuid)));
    }

    uint8_t lmac[6];
    interface_ref iface = NULL;
    int proxyfd = net_init_proxy(opts, uuid, lmac);
    if (-1 == proxyfd) {
        iface = net_init_vmnet(mode, uuid, lmac);
        if (NULL == iface)
            return -1;
    }

    net_vnet_fd_init(peer, "bridge", name, lmac, iface, proxyfd);

    return 0;
}

void net_vnet_set_hw_mac(NetClientState *nc, uint8_t *hw_mac)
{
    VnetState *s = DO_UPCAST(VnetState, nc, nc);
    if (!s)
        return;
    memcpy(s->hw_mac, hw_mac, ETHER_ADDR_LEN);
    s->hw_mac_set = true;
}
