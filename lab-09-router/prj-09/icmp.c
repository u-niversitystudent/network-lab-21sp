#include "icmp.h"
#include "base.h"
#include "ip.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code) {
  // OK
  // malloc and send icmp packet

  struct iphdr *in_ih = packet_to_ip_hdr(in_pkt);

#ifdef SHOW_RAW_REPLY
  if (type == ICMP_ECHOREPLY) {
    for (int i = 0; i < IP_HDR_SIZE(in_ih) + ICMP_COPIED_DATA_LEN; ++i) {
      printf("%u", *(char *)(in_ih + i));
    }
    printf("\n");
  }
#endif

  // len
  int out_len;
  if (type == ICMP_ECHOREPLY) {    /* recv ping current iface:
                                    * copy original content,
                                    * add ip hdr for the reply pkt
                                    */
    out_len = len                  // original length
              - IP_HDR_SIZE(in_ih) // no 0x0000
              + IP_BASE_HDR_SIZE;  // ip header
  } else {
    out_len = ETHER_HDR_SIZE          // ether header
              + IP_BASE_HDR_SIZE      // ip header
              + ICMP_HDR_SIZE         // type + code + checksum
              + IP_HDR_SIZE(in_ih)    // copy packet's ih
              + ICMP_COPIED_DATA_LEN; // copy next 8 byte
  }

  // out-going packet
  char *out_pkt = (char *)malloc(out_len * sizeof(char));
  // out-going packet's ip header
  struct iphdr *out_ih = packet_to_ip_hdr(out_pkt);
  ip_init_hdr(out_ih, 0, ntohl(in_ih->saddr), out_len - ETHER_HDR_SIZE, 1);
  // out-going packet's icmp header
  struct icmphdr *out_imh = (struct icmphdr *)(IP_DATA(out_ih));
  out_imh->type = type;
  out_imh->code = code;

  // fill content
  char *out_content = (char *)out_imh + ICMP_HDR_SIZE;
  if (type == ICMP_ECHOREPLY) {
    int offset_ping = ETHER_HDR_SIZE + IP_HDR_SIZE(in_ih) + 4;
    int len_ping = len - offset_ping;
    memcpy(out_content - 4,      // no 0x0000
           in_pkt + offset_ping, // iphdr true head
           len_ping);            // size of content
  } else {
    memset(out_content - sizeof(char) * 4, // need no 'u' content
           0, sizeof(char) * 4);
    memcpy(out_content, in_ih, IP_HDR_SIZE(in_ih) + ICMP_COPIED_DATA_LEN);
  }

  out_imh->checksum =
      icmp_checksum(out_imh, out_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);

  ip_send_packet(out_pkt, out_len);
}
