#ifndef __MOSPF_DATABASE_H__
#define __MOSPF_DATABASE_H__

#include "base.h"
#include "list.h"

#include "mospf_proto.h"
#include "rtable.h"


#define GRAPH_SIZE 4

extern struct list_head mospf_db;

typedef struct {
    struct list_head list;
    u32 rid; // router which sends the LSU message
    u16 seq; // sequence number of the LSU message
    int nadv; // number of advertisement int->u32
    int alive; // alive for #(seconds)
    struct mospf_lsa *array; // (network, mask, rid)
} mospf_db_entry_t;

typedef struct VerRes {
    int size;
    u32 *verList;
} VerRes_t;

void init_mospf_db();

void dump_mospf_db(void *param);

int rid_to_index(const u32 *verList, int size, u32 rid);

VerRes_t find_vertices(int num);

void *create_graph(u32 *verList, int size);

int min_dist(const int dist[], const int visited[], int num);

iface_info_t *rid_to_iface(u32 rid);

rt_entry_t *dest_mask_to_rtable(u32 dest, u32 mask);

void dij_algo_update_rtable(int num);

#endif
