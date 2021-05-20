//
// Created by zheng on 5/17/2021.
//

#include <stdio.h>
#include <time.h>

#include "trie.h"
#include "ip.h"
#include "general.h"
#include "fread.h"

const u64 zero_64 = 0;

void trie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask,
          u32 *s_port, u32 *a_port,
          int test_lower_bound, int test_upper_bound) {

    int test_num = test_upper_bound - test_lower_bound;

    // read dataset, prepare buffer
    memset(s_ip, 0, sizeof(u32) * NUM_REC);
    memset(s_mask, 0, sizeof(u32) * NUM_REC);
    memset(s_port, 0, sizeof(u32) * NUM_REC);
    memset(a_port, 0, sizeof(u32) * NUM_REC);
    read_all_data(fptr, path, s_ip, s_mask, s_port);

    trie_node_t *root = pt_new_node();
    for (int i = test_lower_bound; i < test_upper_bound; ++i) {
        pt_insert_node(root, s_ip[i], s_mask[i], s_port[i]);
    }

    // for cmp group:
    // trie_node_t *tmp = pt_new_node(); tmp->port=0xFFFF;
    trie_node_t *tmp=NULL;

    struct timespec
            time_start = {0, 0},
            time_end = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time_start);

    for (int i = test_lower_bound; i < test_upper_bound; ++i) {
//        tmp = pt_find_route(root, s_ip[i]);
        a_port[i] = tmp ? tmp->port : 0xFFFF;
    }

    clock_gettime(CLOCK_REALTIME, &time_end);

    double interval = ((double) time_end.tv_sec
                       - (double) time_start.tv_sec) * 1000000000 / test_num
                      + ((double) time_end.tv_nsec -
                         (double) time_start.tv_nsec) / test_num;


    int count = 0;
    for (int i = test_lower_bound; i < test_upper_bound; ++i)
        if (s_port[i] != a_port[i]) count++;

    // summary
    printf("--------\n"
           "Summary for %d times' lookups:\n"
           "diff:\t %d times.\n"
           "time:\t %.5lf ns per lookup.\n",
           test_num, count, interval);

}

int pt_insert_node(trie_node_t *head, u32 ip, u32 mask, u32 port) {
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
    }
    current->match = 1;
    current->port = port;
    // DEBUG info (not used anymore):
    // current->ip = ip & (IP_BCAST << (IP_LEN - mask));
    return 0;
}

trie_node_t *pt_find_route(trie_node_t *root, u32 ip) {
    trie_node_t *found = NULL, *current = root;
    for (int i = 0; i < IP_LEN; ++i) {
        if (current == NULL) break;

        if ((ip << i) & IP_HIGH) { // 1
            if (IS_NULL(current->rchild)) break;
            else {
                current = current->rchild;
                u32 cmp = (u32) ((ip | zero_64) << (i + 1));
                if (current->match == 1 && cmp == 0)
                    found = current;
            }
        } else { // 0
            if (IS_NULL(current->lchild)) break;
            else {
                current = current->lchild;
                u32 cmp = (u32) ((ip | zero_64) << (i + 1));
                if (current->match == 1 && cmp == 0)
                    found = current;
            }
        }
    }
    return found;
}

// only for test
u32 pt_find_route_with_mask(trie_node_t *root, u32 ip, u32 mask) {
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
            printf("[Matched=%d] Port: %d\n", head->match, head->port);
        pt_dump(head->rchild);
    }
}