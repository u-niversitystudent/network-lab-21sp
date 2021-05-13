#include "arp.h"
#include "arpcache.h"
#include "base.h"
#include "ether.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ip.h"

static void ether_init_arp(struct ether_arp *arp, iface_info_t *iface,
                           u16 arp_op, u32 tpa) {
  arp->arp_hdr = htons(ARPHDR_ETHER);
  arp->arp_pro = htons(ETH_P_IP);
  arp->arp_hln = (u8)ETH_ALEN;
  arp->arp_pln = (u8)IP_ALEN;
  arp->arp_op = htons(arp_op);
  memcpy(arp->arp_sha, iface->mac, ETH_ALEN);
  arp->arp_spa = htonl(iface->ip);
  arp->arp_tpa = htonl(tpa);
}

// send an arp request
void arp_send_request(iface_info_t *iface, u32 dst_ip) {
  // OK
  // send arp request when lookup failed in arpcache.

  // Encapsulate an arp request packet
  int len = ETHER_HDR_SIZE + ETHER_ARP_SIZE;
  char *packet = (char *)malloc(len * sizeof(char));
  //  - fill ether header
  struct ether_header *eh = (struct ether_header *)packet;
  memset(eh->ether_dhost, 0xFFFFFF, ETH_ALEN);
  memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
  eh->ether_type = htons(ETH_P_ARP);
  //  - fill arp packet
  struct ether_arp *ea = packet_to_ether_arp(packet);
  ether_init_arp(ea, iface, ARPOP_REQUEST, dst_ip);
  memset(ea->arp_tha, 0, ETH_ALEN);

  // Send it out through iface_send_packet
  iface_send_packet(iface, packet, len);
}

// send an arp reply packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr) {
  // OK
  // send arp reply when receiving arp request

  // Encapsulate an arp reply packet
  int len = ETHER_HDR_SIZE + ETHER_ARP_SIZE;
  char *packet = (char *)malloc(len * sizeof(char));
  //   - fill ether header
  struct ether_header *eh = (struct ether_header *)packet;
  memcpy(eh->ether_dhost, req_hdr->arp_sha, ETH_ALEN);
  memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
  eh->ether_type = htons(ETH_P_ARP);
  //   - fill arp packet
  struct ether_arp *ea = packet_to_ether_arp(packet);
  ether_init_arp(ea, iface, ARPOP_REPLY, ntohl(req_hdr->arp_spa));
  memcpy(ea->arp_tha, req_hdr->arp_tha, ETH_ALEN);

  // Send it out through iface_send_packet
  iface_send_packet(iface, packet, len);
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len) {
  // OK
  // process arp packet: arp request & arp reply.
  struct ether_arp *ea = packet_to_ether_arp(packet);
  if (ntohl(ea->arp_tpa) == iface->ip) {
    switch (ntohs(ea->arp_op)) {
    case ARPOP_REQUEST:
      arp_send_reply(iface, ea);
      break;
    case ARPOP_REPLY:
      arpcache_insert(ntohl(ea->arp_spa), ea->arp_sha);
      break;
    default:
      assert(packet && "PACKET IS NULL");
      free(packet);
    }
  } else {
    assert(packet && "PACKET IS NULL");
    free(packet);
  }
}

// send (IP) packet through arpcache lookup
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet,
                              int len) {
  struct ether_header *eh = (struct ether_header *)packet;
  memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
  eh->ether_type = htons(ETH_P_IP);

  u8 dst_mac[ETH_ALEN];
  // Lookup the mac address of dst_ip in arpcache:
  int found = arpcache_lookup(dst_ip, dst_mac);

  if (found) {
    // If it is found,
    // fill the ethernet header and emit the packet by iface_send_packet;

    log(DEBUG, "found the mac of "IP_FMT", send this packet", LE_IP_FMT_STR(dst_ip));
    memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
    iface_send_packet(iface, packet, len);
  } else {
    // Otherwise,
    // pending this packet into arpcache, and send arp request.

    log(DEBUG, "lookup "IP_FMT" failed, pend this packet", LE_IP_FMT_STR(dst_ip));
    arpcache_append_packet(iface, dst_ip, packet, len);
  }
}
