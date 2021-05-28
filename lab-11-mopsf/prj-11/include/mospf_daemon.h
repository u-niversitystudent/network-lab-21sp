#ifndef __MOSPF_DAEMON_H__
#define __MOSPF_DAEMON_H__

#include "base.h"
#include "types.h"
#include "list.h"
#include "ip.h"
#include "mospf_proto.h"

void mospf_init();

void mospf_run();

void handle_mospf_packet(iface_info_t *iface, char *packet, int len);

void *sending_mospf_hello_thread(void *param);

void *sending_mospf_lsu_thread(void *param);

void *checking_nbr_thread(void *param);

void *checking_database_thread(void *param);

#endif
