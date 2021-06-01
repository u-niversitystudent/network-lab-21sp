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
    fprintf(stdout, "--------------------------------------\n"
                    "MOSPF Database:\n"
                    "Router ID\tNbr Network\tNbr Mask\tNbr RID\n"
                    "--------------------------------------\n");
    list_for_each_entry(pos_db, &mospf_db, list) {
        for (int i = 0; i < pos_db->nadv; i++) {
            fprintf(stdout,
                    IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n",
                    HOST_IP_FMT_STR(pos_db->rid),
                    HOST_IP_FMT_STR(pos_db->array[i].network),
                    HOST_IP_FMT_STR(pos_db->array[i].mask),
                    HOST_IP_FMT_STR(pos_db->array[i].rid)
            );
        }
    }
}

int rid_to_index(const u32 *verList, int size, u32 rid) {
    for (int i = 0; i < size; ++i)
        if (verList[i] == rid) return i;
    return -1;
}

// find vertices for the graph, from mospf_db
VerRes_t find_vertices(int num) {
    VerRes_t ret;

    ret.verList = (u32 *) malloc(num * sizeof(u32));
    memset(ret.verList, 0, num * sizeof(u32));

    // mark head's size
    ret.size = 0;

    // add current instance
    ret.verList[ret.size++] = instance->router_id;

    // add all lsu network
    mospf_db_entry_t *pos_db;
    list_for_each_entry(pos_db, &mospf_db, list) {
        u32 rid = pos_db->rid;
        if (rid == 0) continue;
        if (rid_to_index(ret.verList, ret.size, rid) == -1)
            ret.verList[ret.size++] = rid;

        for (int i = 0; i < pos_db->nadv; ++i) {
            rid = pos_db->array[i].rid;
            if (rid == 0) continue;
            if (rid_to_index(ret.verList, ret.size, rid) == -1)
                ret.verList[ret.size++] = rid;
        }
    }
    return ret;
}

// create graph for vertices
void *create_graph(u32 *verList, int size) {
    int (*graph)[size] =
            (int (*)[size]) malloc(sizeof(int) * size * size);
    memset(graph, 0, sizeof(char) * size * size);

    mospf_db_entry_t *pos_db;
    list_for_each_entry(pos_db, &mospf_db, list) {
        int v0, v1;
        v0 = rid_to_index(verList, size, pos_db->rid);
        for (int i = 0; i < pos_db->nadv; ++i) {
            u32 rid1 = pos_db->array[i].rid;
            if (rid1 == 0) continue;

            v1 = rid_to_index(verList, size, rid1);
            if (v1 == -1) continue;
            graph[v0][v1] = graph[v1][v0] = 1;
        }
    }

    return (void *) graph;
}

// tool for dijkstra algorithm
int min_dist(const int dist[], const int visited[], int num) {
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

void dij(void *in_graph, int dist[], int visited[], int prev[], int num) {
    int (*graph)[num] = in_graph;

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
}

rt_entry_t *dest_mask_to_rtable(u32 dest, u32 mask) {
    rt_entry_t *pos_rt;
    list_for_each_entry(pos_rt, &rtable, list) {
        if (pos_rt->mask == mask)
            if (pos_rt->dest == dest ||
                (pos_rt->dest & pos_rt->mask) == (dest & mask))
                return pos_rt;
    }
    return NULL;
}

void update_rtable_by_db(int max_num) {

    /* load vertices list */

    VerRes_t res = find_vertices(max_num);
    int num = res.size;

#ifdef TEST_FIND_VERTICES
    fprintf(stdout, "--------------------------------------\n"
                    "test find_vertices num=%d\n", num);
    for (int i = 0; i < num; ++i) {
        printf("verList[%d]: "IP_FMT"\n",
               i, HOST_IP_FMT_STR(res.verList[i]));
    }
#endif

    /* prepare graph for dij */

    int(*graph)[num] = create_graph(res.verList, num);

#ifdef TEST_CREATE_GRAPH
    fprintf(stdout, "--------------------------------------\n"
                    "test create_graph num=%d\n", num);
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < num; ++j) {
            printf("%2d ", graph[i][j]);
        }
        printf("\n");
    }
#endif

    /* dijkstra algorithm on $(range) points */

    int dist[num], visited[num], prev[num];
    dij(graph, dist, visited, prev, num);

#ifdef TEST_DIJ_CALC
    fprintf(stdout, "--------------------------------------\n"
                    "test dij's calc num=%d\n"
                    "i\t"
                    "dist[i]\t"
                    "v[i]\t"
                    "prev[i]\n", num);
    for (int i = 0; i < num; ++i) {
        printf("%d\t%d\t%d\t%d\n", i, dist[i], visited[i], prev[i]);
    }
#endif

    /* clear original rtable first */
    clear_rtable();
    load_rtable_from_kernel();

#ifdef TEST_CLEAR_RTABLE
    fprintf(stdout, "--------------------------------------\n"
                    "test clear_rtable num=%d\n");
    print_rtable();
#endif

    /* update rtable */

    mospf_db_entry_t *pos_db;
    list_for_each_entry(pos_db, &mospf_db, list) {
        for (int i = 0; i < pos_db->nadv; i++) {
            // network, mask, rid
            struct mospf_lsa *now = &pos_db->array[i];
            rt_entry_t *renew_rt =
                    dest_mask_to_rtable(now->network, now->mask);
            if (renew_rt) continue;

            // new entry should be added
            int index;

            if (now->rid == 0)
                index = rid_to_index(res.verList, num, pos_db->rid);
            else
                index = rid_to_index(res.verList, num, now->rid);

            if (index == -1) break;

            while (dist[index] > 1 && prev[index] >= 0)
                index = prev[index];

            u32 next_hop_rid = res.verList[index];

            iface_info_t *pos_if;
            mospf_nbr_t *pos_nbr;
            int flag_search_fi = 0;
            list_for_each_entry(
                    pos_if, &instance->iface_list, list) {
                list_for_each_entry(
                        pos_nbr, &pos_if->nbr_list, list) {
                    if (pos_nbr->nbr_id == next_hop_rid) {
                        flag_search_fi = 1;
                        break;
                    }
                }
                if (flag_search_fi) break;
            }

            if (!flag_search_fi) continue;

            u32 gw = pos_nbr->nbr_ip;

            renew_rt = new_rt_entry(
                    pos_db->array[i].network, // dest
                    pos_db->array[i].mask,    // mask
                    gw,                       // gw
                    pos_if,                   // iface
                    RT_CLC);
            add_rt_entry(renew_rt);
        }
    }
}