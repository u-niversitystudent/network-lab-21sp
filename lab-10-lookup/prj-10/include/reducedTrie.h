//
// Created by zheng on 5/18/2021.
//

#ifndef PRJ_10_REDUCEDTRIE_H
#define PRJ_10_REDUCEDTRIE_H

#define IS_RT_INODE(ptr) (*(int *)(ptr)==0)
#define IS_RT_LEAF(ptr) (*(int *)(ptr)!=0)

#include "types.h"

#define NULL_PORT 0xFFFF
#define EXTRACT_BIT(ip, i) ((ip) & (0x80000000 >> (i)))

struct rtInode {
    int isntInode; // 0 = is inode
    int cmpBit;
    void *parent, *leftChild, *rightChild;
};

struct rtLeaf {
    int isLeaf; // 1 = is leaf
    u32 ip;
    u32 port;
    void *parent;
};

void reducedTrie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask, u32 *s_port,
                 u32 *a_port) ;

#endif //PRJ_10_REDUCEDTRIE_H
