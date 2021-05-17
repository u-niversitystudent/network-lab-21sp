//
// Created by zheng on 5/17/2021.
//

#include <stdio.h>
#include <time.h>

#include "trie.h"
#include "ip.h"
#include "general.h"
#include "fread.h"

void trie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask,
          u32 *s_port, u32 *a_port) {

    memset(s_ip, 0, sizeof(u32) * NUM_REC);
    memset(s_mask, 0, sizeof(u32) * NUM_REC);
    memset(s_port, 0, sizeof(u32) * NUM_REC);
    memset(a_port, 0, sizeof(u32) * NUM_REC);

    read_all_data(fptr, path, s_ip, s_mask, s_port);

    // Construct
    trie_node_t *head = pt_new_node();
    for (int i = 0; i < NUM_REC; ++i)
        pt_insert_node(head, s_ip[i], s_mask[i], s_port[i]);

    struct timespec
            time_start = {0, 0},
            time_end = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time_start);

    for (int i = NUM_REC; i > 0; --i)
        a_port[NUM_REC - i] = pt_find_route(head, s_ip[NUM_REC - i],
                                            s_mask[NUM_REC - i]);

    clock_gettime(CLOCK_REALTIME, &time_end);

    double interval = ((double) time_end.tv_sec
                       - (double) time_start.tv_sec) * 1000000000 / NUM_REC
                      + ((double) time_end.tv_nsec -
                         (double) time_start.tv_nsec) / NUM_REC;

    fprintf(stdout, "time per lookup: %.5lf ns.\n", interval);

    printf("res: (0=true, else fault) %d\n", \
    memcmp(s_port, a_port, NUM_REC));

}

int pt_insert_node(trie_node_t *head, u32 ip, u32 mask, u32 port) {
    // Traverse the tree bit by bit
    trie_node_t *current = head;
    for (int i = 0; i < mask; ++i) {
        if ((ip << i) & IP_HIGH) { // 1
            if (IS_NULL(current->rchild)) {
                trie_node_t *nr = pt_new_node();
                current->rchild = nr;
            }
            current = current->rchild;
        } else {
            if (IS_NULL(current->lchild)) {
                trie_node_t *nl = pt_new_node();
                current->lchild = nl;
            }
            current = current->lchild;
        }

        // last layer
        if (i == mask - 1) {
            current->match = 1;
            current->ip = ip & (IP_BCAST << (IP_LEN - mask));
            current->port = port;
            return 0;
        }
    }
    return 1;
}

u32 pt_find_route(trie_node_t *root, u32 ip, u32 mask) {
    trie_node_t *found = NULL, *current = root;
    for (int i = 0; i < mask; ++i) {
        if (current == NULL) break;

        if ((ip << i) & IP_HIGH) { // 1
            if (IS_NULL(current->rchild)) break;
            else current = current->rchild;
        } else { // 0
            if (IS_NULL(current->lchild)) break;
            else current = current->lchild;
        }

        if (i == mask - 1) found = current;
    }

    return found->port;
}

trie_node_t *pt_new_node() {
    trie_node_t *p = (trie_node_t *) malloc(sizeof(trie_node_t));
    memset(p, 0, sizeof(trie_node_t));
    return p;
}

void pt_dump(trie_node_t *head) {
    if (head != NULL) {
        pt_dump(head->lchild);
        if (head->match)
            printf("[Matched=%d] IP: "IP_FMT" Port: %d\n", \
        head->match, LE_IP_FMT_STR(head->ip), head->port);
        pt_dump(head->rchild);
    }
}