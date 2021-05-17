//
// Created by zheng on 5/14/2021.
//

#ifndef PRJ_10_TRIE_H
#define PRJ_10_TRIE_H

#include "types.h"

typedef struct trie_node {
    struct trie_node *lchild, *rchild; // 0, 1

    /* match
     * usage: the node's type,
     * range: 1=match, 0=internal
     * default: 0
     */
    u32 match;

    /* ip
     * usage: current nodes' info;
     * range: 0x00000000~0xFFFFFFFF,
     * default: 0x00000000
     * comment: 'unknown' bits set to 0
     */
    u32 ip;

    /* port:
     * range: 0~7 in static dataset
     * default: 0
     */
    u32 port;
} trie_node_t;

void trie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask, u32 *s_port,
          u32 *a_port);

int pt_insert_node(trie_node_t *head, u32 ip, u32 mask, u32 port);

u32 pt_find_route(trie_node_t *root, u32 ip, u32 mask);

trie_node_t *pt_new_node();

void pt_dump(trie_node_t *head);

#endif //PRJ_10_TRIE_H
