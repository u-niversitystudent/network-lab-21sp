#include "base.h"
#include "stp.h"
#include "types.h"

#include <string.h>

// XXX ifaces are stored in instace->iface_list
extern ustack_t *instance;

extern void iface_send_packet(iface_info_t *iface, const char *packet, int len);

static bool stp_port_not_alt(stp_port_t *p) {
  // alt=0, root/des=1
  return ((p->designated_switch == p->stp->switch_id &&
           p->designated_port == p->port_id) ||
          (p->stp->root_port && p->port_id == p->stp->root_port->port_id));
}

void broadcast_packet(iface_info_t *iface, const char *packet, int len) {
  // OK : broadcast packet
  // fprintf(stdout, "broadcast packet...\n");
  iface_info_t *iface_node, *q;
  list_for_each_entry_safe(iface_node, q, &instance->iface_list, list) {
    if ((iface_node != iface) && stp_port_not_alt(iface_node->port))
      iface_send_packet(iface_node, packet, len);
  }
}
