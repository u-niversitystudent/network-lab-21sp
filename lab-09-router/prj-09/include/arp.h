#ifndef __ARP_H__
#define __ARP_H__

#include "base.h"
#include "ether.h"
#include "types.h"

//#define ARPHRD_ETHER 1
#define ARPHDR_ETHER 1
#define IP_ALEN 4

#define ETHER_ARP_SIZE sizeof(struct ether_arp)

#define ARPOP_REQUEST 1
#define ARPOP_REPLY 2

// Packed: tell compiler to keep the data unaligned
struct ether_arp {
  u16 arp_hdr;          // ARP Header
  u16 arp_pro;          // ARP Protocol
  u8 arp_hln;           // HW Address Length
  u8 arp_pln;           // Protocol Address Length
  u16 arp_op;           // ARP opcode (command).
  u8 arp_sha[ETH_ALEN]; // Sender HW Address
  u32 arp_spa;          // Sender Protocol Address
  u8 arp_tha[ETH_ALEN]; // Target HW Address
  u32 arp_tpa;          // Target Protocol Address
} __attribute__((packed));

static inline struct ether_arp *packet_to_ether_arp(const char *packet) {
  return (struct ether_arp *)(packet + ETHER_HDR_SIZE);
}

void handle_arp_packet(iface_info_t *info, char *pkt, int len);
void arp_send_request(iface_info_t *iface, u32 dst_ip);
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *pkt,
                              int len);

#endif
