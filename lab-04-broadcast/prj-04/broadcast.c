#include "base.h"
#include <stdio.h>

extern ustack_t *instance;

void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
    // OK TODO: broadcast packet
    // fprintf(stdout, "TODO: broadcast packet.\n");
    iface_info_t *iface_node;
    list_for_each_entry(iface_node, &instance->iface_list, list)
    {
        if (iface_node != iface)
        {
            iface_send_packet(iface_node, packet, len);
            // printf("Send to %s!\n", iface_node->name);
        }
    }
}
