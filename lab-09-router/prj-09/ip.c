#include "ip.h"

#include "arp.h"
#include "icmp.h"
#include "rtable.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// handle ip packet
void handle_ip_packet(iface_info_t *iface, char *packet, int len) {
  // OK
  struct iphdr *ih = packet_to_ip_hdr(packet); // iphdr
  u32 dest_ip = ntohl(ih->daddr);              // IP of destination
  u32 iface_ip = iface->ip;                    // IP of recv iface

  if (dest_ip == iface_ip) {
    // if the dest IP addr is equal to the IP addr of the iface
    struct icmphdr *icmp_echo = (struct icmphdr *)(IP_DATA(ih));
    if (icmp_echo->type == ICMP_ECHOREQUEST) {
      // if the packet is ICMP echo request
      //   send ICMP echo reply;
      icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
    }
    else
      free(packet);
  } else {
    // otherwise,
    //   FORWARD THE PACKET.
    rt_entry_t *rt = longest_prefix_match(dest_ip);
    if (rt == NULL) {
      icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
    } else {
      ih->ttl -= 1;
      if (ih->ttl == 0) {
        icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
      } else {
        u32 nxt_hop = rt->gw;
        if (nxt_hop == 0)
          nxt_hop = dest_ip;
        ih->checksum = ip_checksum(ih);
        iface_send_packet_by_arp(rt->iface, nxt_hop, packet, len);
      }
    }
  }
}
