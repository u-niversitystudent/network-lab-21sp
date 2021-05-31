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

#define P1_DEBUG 0

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
    return NULL;
}

void send_mospf_lsu(void *param) {
    // OK: send mOSPF LSU message
    // fprintf(stdout, "TODO: send mOSPF LSU message.\n");
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
#if P1_DEBUG
    printf("nadv=%d\n", nadv);
#endif
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
    return NULL;
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

        dump_mospf_db(NULL);

        // TODO: update route table
        update_rtable_by_db(GRAPH_SIZE);
        print_rtable();

        pthread_mutex_unlock(&mospf_lock);

        sleep(1);
    }
    return NULL;
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
    return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len) {
    // OK: handle mOSPF LSU message
    // fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
    struct iphdr *ih = packet_to_ip_hdr(packet);
    struct mospf_hdr *mh =
            (struct mospf_hdr *) ((char *) ih + IP_HDR_SIZE(ih));
    struct mospf_lsu *mLsu =
            (struct mospf_lsu *) ((char *) mh + MOSPF_HDR_SIZE);
    struct mospf_lsa *mLsaArray =
            (struct mospf_lsa *) ((char *) mLsu + MOSPF_LSU_SIZE);

    u32 rid_pkt = ntohl(mh->rid);
    // recv pkt send by myself -> no need to handle
    if (rid_pkt == instance->router_id) return;

    // start handle
    pthread_mutex_lock(&mospf_lock);

    u16 seq_pkt = ntohs(mLsu->seq);
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
        to_update->rid = ntohl(ref->rid);
        to_update->mask = ntohl(ref->mask);
        to_update->network = ntohl(ref->network);
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
