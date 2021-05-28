#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "types.h"
#include "log.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

#define ROUTER_NUM 4
int graph[ROUTER_NUM][ROUTER_NUM];
int current_num;
u32 router_list[ROUTER_NUM];


void mospf_init() {
    pthread_mutex_init(&mospf_lock, NULL);

    instance->area_id = 0;
    // get the ip address of the first interface
    iface_info_t *iface = list_entry(instance->iface_list.next,
                                     iface_info_t, list);
    instance->router_id = iface->ip;
    instance->sequence_num = 0;
    instance->lsuint = MOSPF_DEFAULT_LSUINT;

    iface = NULL;
    list_for_each_entry(iface, &instance->iface_list, list) {
        iface->helloint = MOSPF_DEFAULT_HELLOINT;
        init_list_head(&iface->nbr_list);
    }

    init_mospf_db();
}

void mospf_run() {
    pthread_t hello, lsu, nbr, db;
    pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
    pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
    pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
    pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param) {
    while (1) { // periodically
        // OK: send mOSPF Hello message periodically
        // NEED CHECK
        // fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
        pthread_mutex_lock(&mospf_lock);
        iface_info_t *pos_iface, *q_iface;
        list_for_each_entry_safe(pos_iface, q_iface,
                                 &instance->iface_list, list) {
            int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE
                      + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
            char *packet = (char *) malloc(len);
            bzero(packet, len);

            struct ether_header *eh = (struct ether_header *) packet;
            struct iphdr *ih = packet_to_ip_hdr(packet);
            struct mospf_hdr *mh = (struct mospf_hdr *) (
                    (char *) ih + IP_BASE_HDR_SIZE);
            struct mospf_hello *mHl = (struct mospf_hello *) (
                    (char *) mh + MOSPF_HDR_SIZE);


            mospf_init_hello(mHl, pos_iface->mask);

            mospf_init_hdr(mh, MOSPF_TYPE_HELLO,
                           MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE,
                           instance->router_id,
                           0);
            mh->checksum = mospf_checksum(mh);

            ip_init_hdr(ih, pos_iface->ip,
                        MOSPF_ALLSPFRouters,
                        IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE,
                        IPPROTO_MOSPF);
            ih->checksum = ip_checksum(ih);

            eh->ether_type = htons(ETH_P_IP);
            memcpy(eh->ether_shost, pos_iface->mac, ETH_ALEN);
            u8 all_route_MAC[ETH_ALEN] = {0x01, 0x00, 0x5e,
                                          0x00, 0x00, 0x05};
            memcpy(eh->ether_dhost, all_route_MAC, ETH_ALEN);

            iface_send_packet(pos_iface, packet, len);
        }
        pthread_mutex_unlock(&mospf_lock);
        sleep(MOSPF_DEFAULT_HELLOINT);
    }
    // return NULL;
}

void send_mospf_lsu(void *param) {
    // OK: send mOSPF LSU message
    fprintf(stdout, "TODO: send mOSPF LSU message.\n");
    iface_info_t *pos_iface, *q_iface;

    int nadv = 0;
    list_for_each_entry_safe(pos_iface, q_iface,
                             &instance->iface_list, list) {
        if (pos_iface->num_nbr) {
            nadv += pos_iface->num_nbr;
        } else {
            nadv += 1;
        }
    }
    printf("nadv=%d\n", nadv);
    struct mospf_lsa *lsaArray = (
            struct mospf_lsa *) malloc(nadv * MOSPF_LSA_SIZE);
    struct mospf_lsa *current = lsaArray;
    list_for_each_entry_safe(pos_iface, q_iface,
                             &instance->iface_list, list) {
        if (pos_iface->num_nbr != 0) {
            mospf_nbr_t *pos_nbr, *q_nbr;
            list_for_each_entry_safe(pos_nbr, q_nbr,
                                     &pos_iface->nbr_list, list) {
                current->rid = htonl(pos_nbr->nbr_id);
                current->mask = htonl(pos_nbr->nbr_mask);
                current->network = htonl(pos_nbr->nbr_ip
                                         & pos_nbr->nbr_mask);
                current++;
            }
        } else {
            current->rid = htonl(0);
            current->mask = htonl(pos_iface->mask);
            current->network = htonl(
                    pos_iface->ip & pos_iface->mask);
            current++;
        }
    }

    int len_lsu_pkt = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE
                      + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE
                      + MOSPF_LSA_SIZE * nadv;
    list_for_each_entry_safe(pos_iface, q_iface,
                             &instance->iface_list, list) {
        if (!pos_iface->num_nbr) continue;
        mospf_nbr_t *pos_nbr, *q_nbr;
        list_for_each_entry_safe(pos_nbr, q_nbr,
                                 &pos_iface->nbr_list, list) {
            char *packet = (char *) malloc(len_lsu_pkt);
            struct iphdr *ih = packet_to_ip_hdr(packet);
            struct mospf_hdr *mh = (struct mospf_hdr *) (
                    (char *) ih + IP_BASE_HDR_SIZE);
            struct mospf_lsu *mLsu = (struct mospf_lsu *) (
                    (char *) mh + MOSPF_HDR_SIZE);
            struct mospf_lsa *mLsa = (struct mospf_lsa *) (
                    (char *) mLsu + MOSPF_LSU_SIZE);

            memcpy(mLsa, lsaArray, MOSPF_LSA_SIZE * nadv);
            mospf_init_lsu(mLsu, nadv);
            mospf_init_hdr(mh, MOSPF_TYPE_LSU,
                           MOSPF_HDR_SIZE + MOSPF_LSU_SIZE
                           + MOSPF_LSA_SIZE * nadv,
                           instance->router_id, 0);
            mh->checksum = mospf_checksum(mh);

            ip_init_hdr(ih, pos_iface->ip, pos_nbr->nbr_ip,
                        len_lsu_pkt - ETHER_HDR_SIZE,
                        IPPROTO_MOSPF);
            ih->checksum = ip_checksum(ih);

            ip_send_packet(packet, len_lsu_pkt);
        }
    }
    free(lsaArray);
    instance->sequence_num += 1;
}

void *checking_nbr_thread(void *param) {
    // OK: neighbor list timeout operation
    // fprintf(stdout, "TODO: neighbor list timeout operation.\n");
    while (1) {
        pthread_mutex_lock(&mospf_lock);

        iface_info_t *pos_iface, *q_iface;
        list_for_each_entry_safe(pos_iface, q_iface,
                                 &instance->iface_list, list) {
            mospf_nbr_t *pos_nbr, *q_nbr;
            list_for_each_entry_safe(pos_nbr, q_nbr,
                                     &pos_iface->nbr_list, list) {
                if (pos_nbr->alive > 3 * MOSPF_DEFAULT_HELLOINT) {
                    list_delete_entry(&pos_nbr->list);
                    free(pos_nbr);
                    pos_iface->num_nbr--;
                    // update link status
                    send_mospf_lsu(NULL);
                }
            }
        }

        pthread_mutex_unlock(&mospf_lock);
        sleep(1);
    }
    // return NULL;
}

int get_router_list_index(u32 rid) {
    for (int i = 0; i < current_num; i++)
        if (router_list[i] == rid) return i;
    return -1;
}

void ldb_to_graph() {
    memset(graph, INT8_MAX - 1, sizeof(graph));
    current_num = 1; // itself
    router_list[0] = instance->router_id;

    mospf_db_entry_t *pos_db, *q_db;
    list_for_each_entry_safe(pos_db, q_db, &mospf_db, list) {
        router_list[current_num++] = pos_db->rid;
    }

    list_for_each_entry_safe(pos_db, q_db, &mospf_db, list) {
        int u = get_router_list_index(pos_db->rid);
        for (int i = 0; i < pos_db->nadv; ++i) {
            if (!pos_db->array[i].rid) continue;
            int v = get_router_list_index(pos_db->array[i].rid);
            graph[u][v] = graph[v][u] = 1;
        }
    }
}

int find_next_hop(int i, const int *prev) {
    while (prev[i] != 0) i = prev[i];
    return i;
}


int min_dist(const int *dist, const int *visited, int range) {
    int ret = -1;
    for (int u = 0; u < range; ++u) {
        if (visited[u]) continue;
        // Not visited and nearest
        if (ret == -1 || dist[u] < dist[ret]) ret = u;
    }
    return ret;
}

void dij_algo(int *prev, int *dist) {
    // TODO: dijkstra algorithm on $(range) points
    int visited[ROUTER_NUM];
    memset(dist, INT8_MAX, ROUTER_NUM * 4);
    memset(prev, -1, ROUTER_NUM * 4);
    memset(visited, 0, ROUTER_NUM * 4);

    dist[0] = 0;

    for (int i = 0; i < current_num; ++i) {
        int u = min_dist(dist, visited, current_num);
        visited[u] = 1;
        for (int v = 0; v < current_num; ++v) {
            if (visited[v] == 0
                && graph[u][v] > 0
                && dist[u] + graph[u][v] < dist[v]) {
                dist[v] = dist[u] + graph[u][v];
                prev[v] = u;
            }
        }
    }
}

void dump_mospf_database(void *param) {
    mospf_db_entry_t *pos_db;
    printf("RID\tNETWORK\tMASK\tNBR\n"
           "--------------------------------------\n");
    list_for_each_entry(
            pos_db, &mospf_db, list) {
        for (int i = 0; i < pos_db->nadv; i++) {
            fprintf(stdout, IP_FMT"\t"IP_FMT"\t"
                            IP_FMT"\t"IP_FMT"\n",
                    HOST_IP_FMT_STR(pos_db->rid),
                    NET_IP_FMT_STR(pos_db->array[i].network),
                    NET_IP_FMT_STR(pos_db->array[i].mask),
                    NET_IP_FMT_STR(pos_db->array[i].rid)
            );
        }
    }
}

int check_rtable(u32 network, u32 mask) {
    rt_entry_t *pos_rt;
    list_for_each_entry(pos_rt, &rtable, list) {
        if (pos_rt->dest == network
            && pos_rt->mask == mask)
            return 1;
    }
    return 0;
}

void update_router(const int *prev, const int *dist) {
    int visited[ROUTER_NUM];
    memset(visited, 0, ROUTER_NUM * 4);
    visited[0] = 1;

    rt_entry_t *pos_rt, *q_rt;
    list_for_each_entry_safe(pos_rt, q_rt, &rtable, list) {
        if (pos_rt->gw) remove_rt_entry(pos_rt);
    }

    for (int i = 0; i < current_num; ++i) {
        int t = -1;
        for (int j = 0; j < current_num; ++j) {
            if (visited[j]) continue;
            if (t == -1 || dist[j] < dist[t])t = j;
        }
        visited[t] = 1;

        mospf_db_entry_t *pos_db, *q_db;
        list_for_each_entry_safe(pos_db, q_db, &mospf_db, list) {
            if (pos_db->rid == router_list[t]) {
                int next_hop = find_next_hop(t, prev);
                iface_info_t *pos_if, *q_if;
                u32 gw;
                int found = 0;
                list_for_each_entry_safe(
                        pos_if, q_if, &instance->iface_list, list) {
                    mospf_nbr_t *pos_nbr, *q_nbr;
                    list_for_each_entry_safe(
                            pos_nbr, q_nbr,
                            &pos_if->nbr_list, list) {
                        printf("prev: %d, %d, %d, %d\n", prev[0], prev[1], prev[2], prev[3]);
                        if (pos_nbr->nbr_id == router_list[next_hop]) {
                            found = 1;
                            gw = pos_nbr->nbr_ip;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found)break;
                for (int j = 0; j < pos_db->nadv; ++j) {
                    u32 network = pos_db->array[j].network;
                    u32 mask = pos_db->array[j].mask;
                    if (check_rtable(network, mask) == 0) {
                        rt_entry_t *new_rt = new_rt_entry(
                                network, mask, gw, pos_if);
                        add_rt_entry(new_rt);
                    }
                }
            }
        }
    }
}

void *checking_database_thread(void *param) {
    while (1) {
        // OK: link state database timeout operation
        // fprintf(stdout, "TODO: link state database timeout operation.\n");
        mospf_db_entry_t *pos_db, *q_db;
        pthread_mutex_lock(&mospf_lock);

        list_for_each_entry_safe(pos_db, q_db, &mospf_db, list) {
            pos_db->alive += 1;
            if (pos_db->alive <= MOSPF_DATABASE_TIMEOUT) continue;

            list_delete_entry(&pos_db->list);
            free(pos_db);
        }

        pthread_mutex_unlock(&mospf_lock);

        int prev[ROUTER_NUM], dist[ROUTER_NUM];
        ldb_to_graph();
        dij_algo(prev, dist);
        update_router(prev, dist);

        // dump_mospf_database(NULL);
         print_rtable();

        sleep(1);
    }
    // return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len) {
    // OK: handle mOSPF Hello message
    // fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
    struct iphdr *ih = packet_to_ip_hdr(packet);
    struct mospf_hdr *mh = (struct mospf_hdr *) (
            (char *) ih + IP_HDR_SIZE(ih));
    struct mospf_hello *mHl = (struct mospf_hello *) (
            (char *) mh + MOSPF_HDR_SIZE);
    u32 rid = ntohl(mh->rid);
    u32 ip = ntohl(ih->saddr);
    u32 mask = ntohl(mHl->mask);

    pthread_mutex_lock(&mospf_lock);

    mospf_nbr_t *pos_nbr, *q_nbr;
    list_for_each_entry_safe(pos_nbr, q_nbr,
                             &iface->nbr_list, list) {
        if (pos_nbr->nbr_id == rid) {
            pos_nbr->alive = 0;
            pthread_mutex_unlock(&mospf_lock);
            return;
        }
    }
    mospf_nbr_t *new_nbr = (mospf_nbr_t *) malloc(sizeof(mospf_nbr_t));
    new_nbr->alive = 0;
    new_nbr->nbr_id = rid;
    new_nbr->nbr_ip = ip;
    new_nbr->nbr_mask = mask;
    list_add_tail(&new_nbr->list, &iface->nbr_list);
    iface->num_nbr += 1;
    send_mospf_lsu(NULL);

    pthread_mutex_unlock(&mospf_lock);

}

void *sending_mospf_lsu_thread(void *param) {
    // send mOSPF LSU message periodically
    // fprintf(stdout, "TODO: send mOSPF LSU message *periodically*.\n");
    while (1) {
        pthread_mutex_lock(&mospf_lock);
        send_mospf_lsu(param);
        pthread_mutex_unlock(&mospf_lock);
        sleep(MOSPF_DEFAULT_LSUINT);
    }
    //return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len) {
    // OK: handle mOSPF LSU message
    fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
    struct iphdr *ih = packet_to_ip_hdr(packet);
    struct mospf_hdr *mh = (struct mospf_hdr *) (
            (char *) ih + IP_HDR_SIZE(ih));
    struct mospf_lsu *mLsu = (struct mospf_lsu *) (
            (char *) mh + MOSPF_HDR_SIZE);
    struct mospf_lsa *mLsaArray = (struct mospf_lsa *) (
            (char *) mLsu + MOSPF_LSU_SIZE);

    u32 rid_pkt = ntohl(mh->rid);
    // recv pkt send by myself -> no need to handle
    if (rid_pkt == instance->router_id) return;

    // start handle
    pthread_mutex_lock(&mospf_lock);

    u16 seq_pkt = ntohl(mLsu->seq);
    u32 nadv_pkt = ntohl(mLsu->nadv);

    mospf_db_entry_t *pos_db, *q_db, *renew_db = NULL;
    list_for_each_entry_safe(pos_db, q_db, &mospf_db, list) {
        if (pos_db->rid == rid_pkt) {
            if (pos_db->seq >= seq_pkt) {
                // out-dated pkt, throw
                pthread_mutex_unlock(&mospf_lock);
                return;
            }
            renew_db = pos_db;
        }
    }
    fprintf(stdout, "TODO: handle mOSPF LSU message 11111.\n");
    if (renew_db == NULL) {
        renew_db = (mospf_db_entry_t *) malloc(
                sizeof(mospf_db_entry_t));
        renew_db->rid = rid_pkt;
        renew_db->array = (struct mospf_lsa *) malloc(
                MOSPF_LSA_SIZE * nadv_pkt);
        list_add_tail(&renew_db->list, &mospf_db);
    }

    // update
    renew_db->alive = 0;
    renew_db->seq = seq_pkt;
    renew_db->nadv = nadv_pkt;
    struct mospf_lsa
            *to_update = renew_db->array,
            *ref = mLsaArray;
    for (int i = 0; i < nadv_pkt; ++i) {
        to_update->rid = ref->rid;
        to_update->mask = ref->mask;
        to_update->network = ref->network;
        to_update++;
        ref++;
    }
    pthread_mutex_unlock(&mospf_lock);

    // update packet's TTL,
    // and decide if should transfer it
    if (--mLsu->ttl) {
        mh->checksum = mospf_checksum(mh);
        pthread_mutex_lock(&mospf_lock);
        iface_info_t *pos_if, *q_if;
        list_for_each_entry_safe(pos_if, q_if,
                                 &instance->iface_list, list) {
            if (pos_if->index == iface->index)
                continue;

            if (pos_if->num_nbr != 0) {
                mospf_nbr_t *pos_nbr, *q_nbr;
                list_for_each_entry_safe(pos_nbr, q_nbr,
                                         &pos_if->nbr_list, list) {
                    char *new_pkt = (char *) malloc(len);
                    memcpy(new_pkt, packet, len);
                    struct iphdr *new_ih = packet_to_ip_hdr(new_pkt);
                    new_ih->saddr = htonl(pos_if->ip);
                    new_ih->daddr = htonl(pos_nbr->nbr_ip);
                    new_ih->checksum = ip_checksum(new_ih);
                    ip_send_packet(new_pkt, len);
                }
            }
        }
        pthread_mutex_unlock(&mospf_lock);
    }
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len) {
    struct iphdr *ip = (struct iphdr *) (packet + ETHER_HDR_SIZE);
    struct mospf_hdr *mospf = (struct mospf_hdr *) (
            (char *) ip + IP_HDR_SIZE(ip));

    if (mospf->version != MOSPF_VERSION) {
        log(ERROR, "received mospf packet with incorrect version (%d)",
            mospf->version);
        return;
    }
    if (mospf->checksum != mospf_checksum(mospf)) {
        log(ERROR, "received mospf packet with incorrect checksum");
        return;
    }
    if (ntohl(mospf->aid) != instance->area_id) {
        log(ERROR, "received mospf packet with incorrect area id");
        return;
    }

    switch (mospf->type) {
        case MOSPF_TYPE_HELLO:
            handle_mospf_hello(iface, packet, len);
            break;
        case MOSPF_TYPE_LSU:
            handle_mospf_lsu(iface, packet, len);
            break;
        default:
            log(ERROR, "received mospf packet with unknown type (%d).",
                mospf->type);
            break;
    }
}
