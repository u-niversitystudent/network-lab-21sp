#include "base.h"
#include <stdio.h>

// XXX ifaces are stored in instace->iface_list
extern ustack_t *instance;

extern void iface_send_packet(iface_info_t *iface, const char *packet, int len);

void broadcast_packet(iface_info_t *iface, const char *packet, int len) {
  // OK : broadcast packet
  // fprintf(stdout, "broadcast packet...\n");
  iface_info_t *iface_node;
  list_for_each_entry(iface_node, &instance->iface_list, list) {
    if (iface_node != iface)
      iface_send_packet(iface_node, packet, len);
  }
}
