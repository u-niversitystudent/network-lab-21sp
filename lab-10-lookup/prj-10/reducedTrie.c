//
// Created by zheng on 5/18/2021.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "reducedTrie.h"
#include "ip.h"
#include "fmt.h"
#include "fread.h"
#include "general.h"

struct rtInode *new_rtInode() {
    struct rtInode *p = (struct rtInode *) malloc(sizeof(struct rtInode));
    memset(p, 0, sizeof(struct rtInode));
    p->isntInode = 0;
    return p;
}

struct rtLeaf *new_rtLeaf() {
    struct rtLeaf *q = (struct rtLeaf *) malloc(sizeof(struct rtLeaf));
    memset(q, 0, sizeof(struct rtLeaf));
    q->isLeaf = 1;
    q->port = NULL_PORT;
    return q;
}

int shared_prefix(u32 ip1, u32 ip2) {
    int i;
    for (i = 0; i < 32; ++i) {
        if (EXTRACT_BIT(ip1, i) != EXTRACT_BIT(ip2, i)) break;
    }
    return (i - 1); // -1 when 0-bit not match
}

int rt_Insert(struct rtInode *root, u32 ip, u32 mask, u32 port) {
    u32 maskedIP = ip & (IP_BCAST << (IP_LEN - mask));
    void *current = root;
    while (current) {
        // INode or  Leaf
        if (IS_RT_INODE(current)) { // inode 'p'
            struct rtInode *p =
                    (struct rtInode *) current;

            // GO LEFT or RIGHT
            if (EXTRACT_BIT(maskedIP, p->cmpBit)) {
                // go right
                if (IS_NULL(p->rightChild)) {
                    // current has no rChild
                    struct rtLeaf *new = new_rtLeaf();
                    new->ip = maskedIP;
                    new->port = port;
                    new->parent = current;
                    p->rightChild = (void *) new;
                    return 1;
                } else { // current have right child
                    current = (void *) p->rightChild;
                }
            } else {
                // go left
                if (IS_NULL(p->leftChild)) {
                    // current has no lChild
                    struct rtLeaf *new = new_rtLeaf();
                    new->ip = maskedIP;
                    new->port = port;
                    new->parent = current;
                    p->leftChild = (void *) new;
                    return 1;
                } else { // current have right child
                    current = (void *) p->leftChild;
                }
            }
        } else {
            // leaf
            struct rtLeaf *q =
                    (struct rtLeaf *) current;

            if (q->ip == maskedIP) {
                // hit leaf and renew ip
                q->port = port;
            } else {
                // split leaf and add a cmp node
                int share_len = shared_prefix(q->ip, maskedIP);

                struct rtLeaf *new_leaf = new_rtLeaf();
                new_leaf->ip = maskedIP;
                new_leaf->port = port;
                struct rtInode *new_inode = new_rtInode();

                struct rtInode *grandparent = (struct rtInode *) q->parent;
                if (grandparent->leftChild == q)
                    grandparent->leftChild = new_inode;
                else grandparent->rightChild = new_inode;

                new_inode->parent = grandparent;

                new_inode->cmpBit = share_len + 1;
                new_inode->parent = current;
                if (EXTRACT_BIT(maskedIP, share_len + 1)) {
                    new_inode->leftChild = current;
                    new_inode->rightChild = (void *) new_leaf;
                } else {
                    new_inode->leftChild = (void *) new_leaf;
                    new_inode->rightChild = current;
                }
                new_leaf->parent = new_inode;
                q->parent = new_inode;

                return 1;
            }
        }
    }
    return 0;
}

void rt_dump(void *ptr) {
    if (ptr == NULL) return;
    if (IS_RT_INODE(ptr)) {
        printf("compare bit %d\n", ((struct rtInode *) ptr)->cmpBit);
        rt_dump(((struct rtInode *) ptr)->leftChild);
        rt_dump(((struct rtInode *) ptr)->rightChild);
    } else {
        printf("ip="IP_FMT" port=%d\n",
               LE_IP_FMT_STR(((struct rtLeaf *) ptr)->ip),
               ((struct rtLeaf *) ptr)->port);
    }
}

void reducedTrie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask, u32 *s_port,
                 u32 *a_port) {
    // read dataset, prepare buffer
    memset(s_ip, 0, sizeof(u32) * NUM_REC);
    memset(s_mask, 0, sizeof(u32) * NUM_REC);
    memset(s_port, 0, sizeof(u32) * NUM_REC);
    memset(a_port, 0, sizeof(u32) * NUM_REC);
    read_all_data(fptr, path, s_ip, s_mask, s_port);

    struct rtInode *root = new_rtInode();
    root->cmpBit = 0;

    for (int i = 0; i < 5; ++i) // up bound should be NUM_REC
    {
        rt_Insert(root, s_ip[i], s_mask[i], s_port[i]);
    }

    rt_dump((void *) root);
    // for cmp group:
    // * trie_node_t *tmp = pt_new_node(); tmp->port=0xFFFF;
    // trie_node_t *tmp;

#if 0

    struct timespec
            time_start = {0, 0},
            time_end = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time_start);


    for (int i = 0; i < NUM_REC; ++i) {
        tmp = pt_find_route(root, s_ip[i]);
        a_port[i] = tmp ? tmp->port : 0xFFFF;
    }


    clock_gettime(CLOCK_REALTIME, &time_end);

    double interval = ((double) time_end.tv_sec
                       - (double) time_start.tv_sec) * 1000000000 / NUM_REC
                      + ((double) time_end.tv_nsec -
                         (double) time_start.tv_nsec) / NUM_REC;


    int count = 0;
    for (int i = 0; i < NUM_REC; ++i)
        if (s_port[i] != a_port[i]) count++;
#endif

    // summary
#if 0
    printf("--------\n"
           "Summary for %d times' lookups:\n"
           "diff:\t %d times.\n"
           "time:\t %.5lf ns per lookup.\n",
           NUM_REC, count, interval);
#endif
}
