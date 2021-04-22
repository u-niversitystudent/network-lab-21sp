#include "mac.h"
#include "log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

mac_port_map_t mac_port_map;

// initialize mac_port table
void init_mac_port_table() {
  bzero(&mac_port_map, sizeof(mac_port_map_t));

  for (int i = 0; i < HASH_8BITS; i++)
    init_list_head(&mac_port_map.hash_table[i]);

  pthread_mutex_init(&mac_port_map.lock, NULL);

  pthread_create(&mac_port_map.thread, NULL, sweeping_mac_port_thread, NULL);
}

// destroy mac_port table
void destroy_mac_port_table() {
  pthread_mutex_lock(&mac_port_map.lock);
  mac_port_entry_t *entry, *q;
  for (int i = 0; i < HASH_8BITS; i++) {
    list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
      list_delete_entry(&entry->list);
      free(entry);
    }
  }
  pthread_mutex_unlock(&mac_port_map.lock);
}

// sup the mac address in mac_port table
iface_info_t *lookup_port(u8 mac[ETH_ALEN]) {
  // OK : implement the lookup process here
  // fprintf(stdout, "Loop up port...\n");

  pthread_mutex_lock(&mac_port_map.lock);

  u8 hash_val = hash8(mac, ETH_ALEN);
  struct list_head *head;
  head = &(mac_port_map.hash_table[hash_val]);

  mac_port_entry_t *pos, *q;
  iface_info_t *iface_found;
  iface_found = NULL;
  list_for_each_entry_safe(pos, q, head, list) {
    if (strcmp((char *)(pos->mac), (char *)mac) == 0) {
      iface_found = pos->iface;
      break;
    }
  }

  pthread_mutex_unlock(&mac_port_map.lock);
  return iface_found;
}

// insert the mac -> iface mapping into mac_port table
void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface) {
  // OK : implement the insertion process here
  // fprintf(stdout, "Insert mac port...\n");

  pthread_mutex_lock(&mac_port_map.lock);

  u8 hash_val = hash8(mac, ETH_ALEN);
  struct list_head *head;
  head = &(mac_port_map.hash_table[hash_val]);

  mac_port_entry_t *pos, *q;
  list_for_each_entry_safe(pos, q, head, list) {
    if (strcmp((char *)(pos->mac), (char *)mac) == 0) {
      pos->visited = time(NULL);
      pthread_mutex_unlock(&mac_port_map.lock);
      return;
    }
  }

  pos = (mac_port_entry_t *)malloc(sizeof(mac_port_entry_t));
  strcpy((char *)(pos->mac), (char *)mac);
  pos->iface = iface;
  pos->visited = time(NULL);

  list_add_tail(&pos->list, head);

  pthread_mutex_unlock(&mac_port_map.lock);
}

// dumping mac_port table
void dump_mac_port_table() {
  mac_port_entry_t *entry = NULL, *q = NULL;
  time_t now = time(NULL);

  fprintf(stdout, "dumping the mac_port table:\n");
  pthread_mutex_lock(&mac_port_map.lock);
  for (int i = 0; i < HASH_8BITS; i++) {
    list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
      printf("i=%d\n", i);
      fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac),
              entry->iface->name, (int)(now - entry->visited));
    }
  }

  pthread_mutex_unlock(&mac_port_map.lock);
}

// sweeping mac_port table, remove the entry which has not been visited in the
// last 30 seconds.
int sweep_aged_mac_port_entry() {
  // OK TODO: implement the sweeping process here
  pthread_mutex_lock(&mac_port_map.lock);
  time_t now = time(NULL);

  for (int i = 0; i < HASH_8BITS; i++) {
    struct list_head *head;
    head = &mac_port_map.hash_table[i];
    mac_port_entry_t *pos, *q;
    list_for_each_entry_safe(pos, q, head, list) {
      if (now - pos->visited > MAC_PORT_TIMEOUT) {
        list_delete_entry(&pos->list);
        free(pos);
      }
    }
  }

  pthread_mutex_unlock(&mac_port_map.lock);
  return 0;
}

// sweeping mac_port table periodically, by calling sweep_aged_mac_port_entry
void *sweeping_mac_port_thread(void *nil) {
  while (1) {
    sleep(1);
    int n = sweep_aged_mac_port_entry();

    if (n > 0)
      log(DEBUG, "%d aged entries in mac_port table are removed.", n);
  }

  return NULL;
}
