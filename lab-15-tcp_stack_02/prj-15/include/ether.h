#ifndef __ETHER_H__
#define __ETHER_H__

#include "types.h"

#define ETH_ALEN    6      // length of mac address
#define ETH_FRAME_LEN 1514 // max len of an ethernet frame (packet)

// protocol format in ethernet header
#define ETH_P_ALL        0x0003          // every packet, only used when tending to receive all packets
#define ETH_P_IP        0x0800            // IP packet
#define ETH_P_ARP        0x0806            // ARP packet

struct ether_header {
    u8 ether_dhost[ETH_ALEN];            // destination mac address
    u8 ether_shost[ETH_ALEN];            // source mac address
    u16 ether_type;                        // protocol format
};

#define ETHER_HDR_SIZE sizeof(struct ether_header)


#endif
