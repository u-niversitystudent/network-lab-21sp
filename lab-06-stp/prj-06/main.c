#include "base.h"
#include "ether.h"
#include "log.h"
#include "mac.h"
#include "stp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static bool stp_port_not_alt(stp_port_t *p) {
  // alt=0, root/des=1
  return ((p->designated_switch == p->stp->switch_id &&
           p->designated_port == p->port_id) ||
          (p->stp->root_port && p->port_id == p->stp->root_port->port_id));
}

void handle_packet(iface_info_t *iface, char *packet, int len) {
  struct ether_header *eh = (struct ether_header *)packet;
  if (memcmp(eh->ether_dhost, eth_stp_addr, sizeof(*eth_stp_addr)) != 0) {
    // OK : forward this packet, if this lab has merged with 05-switch.
    // fprintf(stdout, ": received non-stp packet, forward it.\n");

    // if shost is from an alter
    if (!stp_port_not_alt(iface->port))
      return;

    insert_mac_port(eh->ether_shost, iface);
    iface_info_t *dest = lookup_port(eh->ether_dhost);

    if (dest != NULL) {
      if (stp_port_not_alt(dest->port))
        iface_send_packet(dest, packet, len);
    } else
      broadcast_packet(iface, packet, len);

    free(packet);

    return;
  }

  stp_port_handle_packet(iface->port, packet, len);

  free(packet);
}

// run user stack, receive packet on each interface, and handle those packet
// like normal switch
void ustack_run() {
  struct sockaddr_ll addr;
  socklen_t addr_len = sizeof(addr);
  char buf[ETH_FRAME_LEN];
  int len;

  while (1) {
    int ready = poll(instance->fds, instance->nifs, -1);
    if (ready < 0) {
      // interrupted by SIGTERM, wait until this program EXIT
      while (1)
        sleep(1);
    } else if (ready == 0)
      continue;

    for (int i = 0; i < instance->nifs; i++) {
      if (instance->fds[i].revents & POLLIN) {
        len = recvfrom(instance->fds[i].fd, buf, ETH_FRAME_LEN, 0,
                       (struct sockaddr *)&addr, &addr_len);
        if (len <= 0) {
          log(ERROR, "receive packet error: %s", strerror(errno));
        } else if (addr.sll_pkttype == PACKET_OUTGOING) {
          // XXX: Linux raw socket will capture both incoming and
          // outgoing packets, while we only care about the incoming ones.

          // log(DEBUG, "received packet which is sent from the "
          // 		"interface itself, drop it.");
        } else {
          iface_info_t *iface = fd_to_iface(instance->fds[i].fd);
          if (!iface)
            continue;

          char *packet = malloc(len);
          if (!packet) {
            log(ERROR, "malloc failed when receiving packet.");
            continue;
          }
          memcpy(packet, buf, len);
          handle_packet(iface, packet, len);
        }
      }
    }
  }
}

int main(int argc, const char **argv) {
  if (getuid() && geteuid()) {
    printf("Permission denied, should be superuser!\n");
    exit(1);
  }

  init_ustack();

  init_mac_port_table();
  stp_init(&instance->iface_list);

  ustack_run();
  destroy_mac_port_table();

  return 0;
}
