#include "mospf_database.h"
#include "ip.h"
#include "list.h"
#include "mospf_nbr.h"

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

struct list_head mospf_db;

void init_mospf_db() {
    init_list_head(&mospf_db);
}

void dump_mospf_db(void *param) {
    mospf_db_entry_t *pos_db;
    fprintf(stdout, "MOSPF Database:\n");
    fprintf(stdout, "Router ID\tNetwork\tMask\tNeighbor\n");
    fprintf(stdout, "--------------------------------------\n");
    list_for_each_entry(
            pos_db, &mospf_db, list) {
        for (int i = 0; i < pos_db->nadv; i++) {
            fprintf(stdout, IP_FMT"\t"IP_FMT"\t"
                            IP_FMT"\t"IP_FMT"\n",
                    HOST_IP_FMT_STR(pos_db->rid),
                    HOST_IP_FMT_STR(pos_db->array[i].network),
                    HOST_IP_FMT_STR(pos_db->array[i].mask),
                    HOST_IP_FMT_STR(pos_db->array[i].rid)
            );
        }
    }
}

int rid_to_index(const u32 verList[], int size, u32 rid) {
    for (int i = 0; i < size; ++i) {
        if (verList[i] == rid) return i;
    }
    return -1;
}


VerRes_t find_vertices(int num) {
    // find vertices for the graph, from mospf_db
    VerRes_t ret;

    ret.verList = (u32 *) malloc(num * sizeof(u32));
    memset(ret.verList, 0, num * sizeof(u32));

    ret.size = 0; // mark head's size

    mospf_db_entry_t *pos_db;
    list_for_each_entry(pos_db, &mospf_db, list) {
        int res_to_lsu =
                rid_to_index(ret.verList, ret.size, pos_db->rid);
        if (res_to_lsu == -1)
            ret.verList[ret.size++] = pos_db->rid;

        struct mospf_lsa *mLsa = pos_db->array;
        for (int i = 0; i < pos_db->nadv; ++i) {
            int res_to_lsa =
                    rid_to_index(ret.verList, ret.size, mLsa->rid);
            if (res_to_lsa == -1) {
                ret.verList[ret.size++] = mLsa->rid;
                mLsa++;
            }
        }
    }
    return ret;
}

// create graph for vertices
void *create_graph(u32 *verList, int size) {
    int (*graph)[size] = (int (*)[size]) malloc(
            sizeof(int) * size * size);
    memset(graph, 0, sizeof(char) * size * size);
    mospf_db_entry_t *pos_db;
    list_for_each_entry(pos_db, &mospf_db, list) {
        int v0 = rid_to_index(verList, size, pos_db->rid), v1;
        for (int i = 0; i < pos_db->nadv; ++i) {
            v1 = rid_to_index(verList, size, pos_db->array[i].rid);
            graph[v0][v1] = graph[v1][v0] = 1;
        }
    }
    return (void *) graph;
}

int min_dist(const int dist[], const int visited[], int num) {
    // tool for dijkstra algorithm
    int min = INT8_MAX, min_index = -1;
    for (int i = 0; i < num; ++i) {
        if (visited[i] == 1) continue;
        if (dist[i] < min) {
            min = dist[i];
            min_index = i;
        };
    }
    return min_index;
}

iface_info_t *rid_to_iface(u32 rid) {
    iface_info_t *pos_if;
    int if_found = 0;

    list_for_each_entry(pos_if, &instance->iface_list, list) {
        mospf_nbr_t *pos_nbr;
        list_for_each_entry(pos_nbr, &pos_if->nbr_list, list) {
            if (rid == pos_nbr->nbr_id) {
                if_found = 1;
                break;
            }
        }
        if (if_found == 1) break;
    }

    if (if_found == 1) return pos_if;
    else return NULL;
}

mospf_nbr_t *rid_to_nbr(u32 rid) {
    iface_info_t *pos_if;
    mospf_nbr_t *pos_nbr;
    list_for_each_entry(pos_if, &instance->iface_list, list) {
        list_for_each_entry(
                pos_nbr, &pos_if->nbr_list, list) {
            if (rid == pos_nbr->nbr_id) {
                return pos_nbr;
            }
        }
    }
    return NULL;
}

rt_entry_t *dest_mask_to_rtable(u32 dest, u32 mask) {
    rt_entry_t *pos_rt;
    list_for_each_entry(pos_rt, &rtable, list) {
        if ((pos_rt->dest & pos_rt->mask) == (dest & mask)) {
            return pos_rt;
        }
    }
    return NULL;
}

void dij_algo_update_rtable(int num) {

    /* dijkstra algorithm on $(range) points */

    VerRes_t res = find_vertices(num);

#ifdef TEST_FIND_VERTICES
    printf("test find_vertices\n");
    for (int i = 0; i < num; ++i) {
        printf("verList[%d]: "IP_FMT"\n",
               i, HOST_IP_FMT_STR(res.verList[i]));
    }
#endif

    int(*graph)[num] = create_graph(res.verList, num);

#ifdef TEST_CREATE_GRAPH
    printf("test create_graph\n");
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < num; ++j) {
            printf("%d ", graph[i][j]);
        }
        printf("\n");
    }
    printf("\n");
#endif

    int dist[num], visited[num], prev[num];
    for (int i = 0; i < num; ++i) dist[i] = INT8_MAX;
    memset(visited, 0, sizeof(int) * num);
    memset(prev, (u8) -1, sizeof(int) * num);

    dist[0] = 0;
    for (int i = 0; i < num; ++i) {
        int next = min_dist(dist, visited, num);
        visited[next] = 1;

        for (int j = 0; j < num; ++j) {
            if (visited[j] == 0
                && graph[next][j] > 0
                && dist[next] + graph[next][j] < dist[j]) {
                dist[j] = dist[next] + graph[next][j];
                prev[j] = next;
            }
        }
    }

#ifdef TEST_DIJ_CALC
    printf("test dij's calc ");
    printf("num=%d\ndist\tvisited\tprev\t", num);
    for (int i = 0; i < num; ++i) {
        printf("%d\t%d\t%d\n", dist[i], visited[i], prev[i]);
    }
#endif

//    printf("check 01\n");

    /* update rtable */
//    for (int i = 0; i < num; ++i) {
//        //  rid => network, mask, gw, iface, flag[cal]
//        if (dist[i] == INT8_MAX)continue;
//
//        mospf_nbr_t *resNextHop = rid_to_nbr(prev[i]);
//        iface_info_t *resIface = rid_to_iface(prev[i]);
//        u32 dest = res.verList[i],
//                mask = resNextHop->nbr_mask,
//                gw = resNextHop->nbr_ip;
//
//        rt_entry_t *old_rt = dest_mask_to_rtable(dest, mask);
//        if(!old_rt) {
//            rt_entry_t *new_rt = new_rt_entry(
//                    dest, mask, gw, resIface, RT_CLC);
//            add_rt_entry(new_rt);
//        } else if (old_rt->flags==RT_CLC){
//            old_rt->dest=dest;
//            old_rt->mask=mask;
//            old_rt->gw=gw;
//            old_rt->iface=resIface;
//            memcpy(old_rt->if_name, resIface->name, sizeof(old_rt->if_name));
//        }
//    }
}