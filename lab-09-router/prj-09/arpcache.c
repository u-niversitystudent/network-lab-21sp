#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ip.h"
#include "list.h"
#include "rtable.h"

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init() {
  bzero(&arpcache, sizeof(arpcache_t));

  init_list_head(&(arpcache.req_list));

  pthread_mutex_init(&arpcache.lock, NULL);

  pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy() {
  pthread_mutex_lock(&arpcache.lock);

  struct arp_req *req_entry = NULL, *req_q;
  list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
    struct cached_pkt *pkt_entry = NULL, *pkt_q;
    list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets),
                             list) {
      list_delete_entry(&(pkt_entry->list));
      free(pkt_entry->packet);
      free(pkt_entry);
    }

    list_delete_entry(&(req_entry->list));
    free(req_entry);
  }

  pthread_kill(arpcache.thread, SIGTERM);

  pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN]) {
  // OK
  // lookup ip address in arp cache

  pthread_mutex_lock(&arpcache.lock);
  // traverse the table to find
  for (int i = 0; i < MAX_ARP_SIZE; ++i) {
    if (arpcache.entries[i].valid == 0)
      continue;

#ifdef SHOW_VALID_ENTRIES
    // usage: print valid entries
    printf("i=%d ip4=", i);
    printf(IP_FMT, LE_IP_FMT_STR(arpcache.entries[i].ip4));
    printf(" in_mac=");
    for (int j = 0; j < ETH_ALEN; ++j)
      printf("%d", mac[j]);

    printf(" entry_mac=");
    for (int j = 0; j < ETH_ALEN; ++j)
      printf("%d", arpcache.entries[i].mac[j]);

    printf("\n");
#endif

    if (arpcache.entries[i].ip4 == ip4) {
      // whether there is an entry with the same IP
      // and mac address with the given arguments
      memcpy(arpcache.entries[i].mac, mac, ETH_ALEN);
      pthread_mutex_unlock(&arpcache.lock);
      return 1;
    }
  }
  pthread_mutex_unlock(&arpcache.lock);
  return 0;
}

// append the packet to arpcache
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet,
                            int len) {
  // OK
  // append the ip address if lookup failed,
  // and send arp request if necessary.
  pthread_mutex_lock(&arpcache.lock);
  // Lookup in the list which stores pending packets:
  struct cached_pkt *new_pkt =
      (struct cached_pkt *)malloc(sizeof(struct cached_pkt));
  new_pkt->packet = packet;
  new_pkt->len = len;

  struct arp_req *pos = NULL, *q;

  list_for_each_entry_safe(pos, q, &(arpcache.req_list), list) {
    if (pos->ip4 == ip4 && pos->iface == iface) {
      // if there is already an entry with the same IP address and iface
      // (which means the corresponding arp request has been sent out),

      // just APPEND THIS PACKET AT THE TAIL OF THAT ENTRY
      // (the entry may contain more than one packet);
      list_add_tail(&(new_pkt->list), &(pos->cached_packets));

      pthread_mutex_unlock(&arpcache.lock);
      return;
    }
  }
  // otherwise,
  // MALLOC A NEW ENTRY WITH THE GIVEN IP ADDR & IFACE,
  struct arp_req *new_req = (struct arp_req *)malloc(sizeof(struct arp_req));
  new_req->ip4 = ip4;
  new_req->iface = iface;
  new_req->retries = ARP_REQUEST_MAX_RETRIES;
  list_add_tail(&(new_req->list), &(arpcache.req_list));
  // APPEND THE PACKET,
  init_list_head(&(new_req->cached_packets));
  list_add_tail(&(new_pkt->list), &(new_req->cached_packets));
  // and SEND ARP REQUEST
  arp_send_request(iface, ip4);
  new_req->sent = time(NULL);

  pthread_mutex_unlock(&arpcache.lock);
}

// insert mapping in cache, send pending pkt in req_list
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN]) {
  // OK
  // insert ip->mac entry, and send all the pending packets.
  pthread_mutex_lock(&arpcache.lock);

  // insert the IP->mac mapping into arpcache
  int etr_avl; // available entry
  for (etr_avl = 0; etr_avl < MAX_ARP_SIZE; ++etr_avl) {
    if (arpcache.entries[etr_avl].valid == 0)
      break;
    if (etr_avl == MAX_ARP_SIZE - 1)
      etr_avl = -1;
  }
  time_t now = time(NULL);
  if (etr_avl == -1)
    etr_avl = now % MAX_ARP_SIZE;

  arpcache.entries[etr_avl].ip4 = ip4;
  arpcache.entries[etr_avl].added = now;
  memcpy(arpcache.entries[etr_avl].mac, mac, ETH_ALEN);
  arpcache.entries[etr_avl].valid = 1;

  struct arp_req *pos_req, *q_req;
  list_for_each_entry_safe(pos_req, q_req, &(arpcache.req_list), list) {
    if (pos_req->ip4 != ip4)
      continue;
    // if there are pending packets waiting for this mapping,

    // FOR EACH OF THEM,
    struct cached_pkt *pos_pkt, *q_pkt;
    list_for_each_entry_safe(pos_pkt, q_pkt, &(pos_req->cached_packets), list) {
      // FILL THE ETHERNET HEADER,
      struct ether_header *eh = (struct ether_header *)(pos_pkt->packet);
      memcpy(eh->ether_dhost, mac, ETH_ALEN);
      // and SEND THEM OUT
      iface_send_packet(pos_req->iface, pos_pkt->packet, pos_pkt->len);
      list_delete_entry(&(pos_pkt->list));
      free(pos_pkt);
    }
    list_delete_entry(&(pos_req->list));
    free(pos_req);
    break;
  }

  pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
void *arpcache_sweep(void *arg) {
  while (1) {
    sleep(1);
    // OK
    // sweep arpcache periodically:
    // remove old entries, resend arp requests
    pthread_mutex_lock(&arpcache.lock);

    // For the IP->mac entry,
    time_t now = time(NULL);
    for (int i = 0; i < MAX_ARP_SIZE; ++i) {
      // if the entry has been in the table for
      // more than 15 seconds, remove it from the table.
      if (arpcache.entries[i].valid == 0)
        continue;
      if (now - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT)
        arpcache.entries[i].valid = 0;
    }

    struct list_head *del_reqs =
        (struct list_head *)malloc(sizeof(struct list_head));
    init_list_head(del_reqs);

    // For the pending packets,
    struct arp_req *pos_req, *q_req;
    list_for_each_entry_safe(pos_req, q_req, &(arpcache.req_list), list) {
      if (now - pos_req->sent > 1) {
        // if the arp request is sent out 1 second ago,
        // while the reply has not been received,
        if (--(pos_req->retries) > 0) {
          // (if the arp request has been sent without receiving
          // arp reply for less than 5 times),

          // RETRANSMIT THE ARP REQUEST
          arp_send_request(pos_req->iface, pos_req->ip4);
          pos_req->sent = now;
        } else {
          // if the arp request has been sent 5 times
          // without receiving arp reply,

          // and DROP THESE PACKET (from arpcache).
          list_delete_entry(&(pos_req->list));
          list_add_tail(&(pos_req->list), del_reqs);
        }
      }
    }

    pthread_mutex_unlock(&arpcache.lock);

    list_for_each_entry_safe(pos_req, q_req, del_reqs, list) {
      struct cached_pkt *pos_pkt, *q_pkt;
      // for each pending packet,
      list_for_each_entry_safe(pos_pkt, q_pkt, &(pos_req->cached_packets),
                               list) {
        // SEND ICMP PACKET (DEST_HOST_UNREACHABLE),
        icmp_send_packet(pos_pkt->packet, pos_pkt->len, ICMP_DEST_UNREACH,
                         ICMP_HOST_UNREACH);
        usleep(100);
        list_delete_entry(&(pos_pkt->list));
        free(pos_pkt->packet);
        free(pos_pkt);
      }
      list_delete_entry(&(pos_req->list));
      free(pos_req);
    }

    free(del_reqs);
  }
}
