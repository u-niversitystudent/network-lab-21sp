//
// Created by zheng on 5/14/2021.
//

#ifndef PRJ_10_TRIE_H
#define PRJ_10_TRIE_H

#include "types.h"

typedef struct trie_node {
    struct trie_node *lchild, *rchild; // 0, 1
    u32 match; // 0=internal node, 1=match node
    u32 port; // default=0, range=0~7 (in file)
} trie_node_t;

void trie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask, u32 *s_port,
          u32 *a_port);

int pt_insert_node(trie_node_t *head, u32 ip, u32 mask, u32 port);

trie_node_t *pt_find_route(trie_node_t *root, u32 ip);

u32 pt_find_route_with_mask(trie_node_t *root, u32 ip, u32 mask);

trie_node_t *pt_new_node();

void pt_dump(trie_node_t *head);

#endif //PRJ_10_TRIE_H
