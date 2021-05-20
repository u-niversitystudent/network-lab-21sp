//
// Created by zheng on 5/18/2021.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "reducedTrie.h"
#include "ip.h"
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

u32 find_max_ip(void *root) {
    u32 max = 0, tmpIP;
    struct rtInode *tmpR, *tmpL;

    while (root != NULL) {
        if (IS_RT_INODE(root)) {
            tmpR = ((struct rtInode *) root)->rightChild;
            tmpL = ((struct rtInode *) root)->leftChild;
            if (tmpR == NULL) root = tmpL;
            else root = tmpR;
        } else {
            tmpIP = ((struct rtLeaf *) root)->ip;
            max = MAX(tmpIP, max);
            break;
        }
    }
    return max;
}

u32 find_min_ip(void *root) {
    u32 min = 0xFFFF, tmpIP;
    struct rtInode *tmpR, *tmpL;

    while (root != NULL) {
        if (IS_RT_INODE(root)) {
            tmpR = ((struct rtInode *) root)->rightChild;
            tmpL = ((struct rtInode *) root)->leftChild;
            if (tmpL == NULL) root = tmpR;
            else root = tmpL;
        } else {
            tmpIP = ((struct rtLeaf *) root)->ip;
            min = MIN(tmpIP, min);
            break;
        }
    }
    return min;
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
                    struct rtLeaf *new_leaf = new_rtLeaf();
                    new_leaf->ip = maskedIP;
                    new_leaf->port = port;
                    new_leaf->mask = mask;
                    new_leaf->parent = current;
                    p->rightChild = (void *) new_leaf;
                    return 1;
                } else {
                    // current have right child

                    if (IS_RT_INODE(p->rightChild)) {
                        // maybe need insert a internal node
                        u32 min_ip = find_min_ip(
                                (struct rtInode *) (p->rightChild));
                        if (maskedIP < min_ip) {
                            int cmp = shared_prefix(maskedIP,
                                                    min_ip);
                            if (cmp <
                                ((struct rtInode *) (p->leftChild))->cmpBit) {
                                struct rtInode *new_inode = new_rtInode();
                                new_inode->cmpBit = cmp + 1;
                                new_inode->rightChild = p->rightChild;
                                new_inode->parent = current;
                                ((struct rtInode *) (p->rightChild))->parent =
                                        new_inode;
                                p->rightChild = (void *) new_inode;
                            }
                        }
                    }
                    current = (void *) p->rightChild;
                }
            } else {
                // go left
                if (IS_NULL(p->leftChild)) {
                    // current has no lChild
                    struct rtLeaf *new_leaf = new_rtLeaf();
                    new_leaf->ip = maskedIP;
                    new_leaf->port = port;
                    new_leaf->mask = mask;
                    new_leaf->parent = current;
                    p->leftChild = (void *) new_leaf;
                    return 1;
                } else {
                    // current have left child

                    if (IS_RT_INODE(p->leftChild)) {
                        // maybe need insert a internal node
                        u32 max_ip = find_max_ip(
                                (struct rtInode *) (p->leftChild));
                        if (maskedIP > max_ip) {
                            int cmp = shared_prefix(maskedIP,
                                                    max_ip);
                            if (cmp <
                                ((struct rtInode *) (p->leftChild))->cmpBit) {
                                struct rtInode *new_inode = new_rtInode();
                                new_inode->cmpBit = cmp + 1;
                                new_inode->leftChild = p->leftChild;
                                new_inode->parent = current;
                                ((struct rtInode *) (p->leftChild))->parent =
                                        new_inode;
                                p->leftChild = (void *) new_inode;
                            }
                        }
                    }
                    current = (void *) p->leftChild;
                }
            }
        } else {
            // leaf
            struct rtLeaf *q =
                    (struct rtLeaf *) current;

            if (q->ip == maskedIP) {
                // hit,
                // decide: change some nodes && renew ip
                struct rtInode *parent = (struct rtInode *) q->parent;
                if (parent->cmpBit >= mask) {
                    return 1;
                } else {
                    // new leaf should
                    if (q->mask == mask) {
                        q->port = port;
                    } else {
                        struct rtInode *new_inode = new_rtInode();
                        struct rtLeaf *new_leaf = new_rtLeaf();
                        if (parent->leftChild == current) {
                            parent->leftChild = new_inode;
                        } else {
                            parent->rightChild = new_inode;
                        }
                        new_inode->parent = parent;
                        new_leaf->ip = ip;
                        new_leaf->port = port;
                        new_leaf->mask = mask;
                        new_leaf->parent = (void *) new_inode;
                        q->parent = (void *) new_inode;
                        if (q->mask < mask) {
                            new_inode->cmpBit = (int) mask;
                            new_inode->leftChild = (void *) new_leaf;
                            new_inode->rightChild = current;
                        } else {
                            new_inode->cmpBit = (int) q->mask;
                            new_inode->leftChild = current;
                            new_inode->rightChild = (void *) new_leaf;
                        }
                    }
                    return 1;
                }

            } else {
                // split leaf and add a cmp node
                int share_len = shared_prefix(q->ip, maskedIP);

                struct rtLeaf *new_leaf = new_rtLeaf();
                new_leaf->ip = maskedIP;
                new_leaf->port = port;
                new_leaf->mask = mask;
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

u32 rt_find_route(void *root, u32 ip) {
    void *current = root;
    u32 port = NULL_PORT;
    while (current) {
        if (IS_RT_INODE(current)) {
            // meet internal node
            if (EXTRACT_BIT(ip, ((struct rtInode *) current)->cmpBit)) {
                current = ((struct rtInode *) (current))->rightChild;
            } else {
                current = ((struct rtInode *) (current))->leftChild;
            }
        } else {
            // meet leaf node
            if ((ip &
                 (IP_BCAST << (IP_LEN - ((struct rtLeaf *) (current))->mask)))
                == (((struct rtLeaf *) (current))->ip)) {
                port = ((struct rtLeaf *) (current))->port;
            }
            current = NULL;
        }
    }
    return port;
}

void rt_dump(void *ptr) {
    if (ptr == NULL) return;
    if (IS_RT_INODE(ptr)) {
//        printf("compare bit %d\n", ((struct rtInode *) ptr)->cmpBit);
        rt_dump(((struct rtInode *) ptr)->leftChild);
        rt_dump(((struct rtInode *) ptr)->rightChild);
    } else {
        printf("ip="IP_FMT" port=%d\n",
               LE_IP_FMT_STR(((struct rtLeaf *) ptr)->ip),
               ((struct rtLeaf *) ptr)->port);
    }
}

void reducedTrie(FILE *fptr, char *path, u32 *s_ip, u32 *s_mask, u32 *s_port,
                 u32 *a_port,
                 int test_lower_bound, int test_upper_bound) {

    // read dataset, prepare buffer
    memset(s_ip, 0, sizeof(u32) * NUM_REC);
    memset(s_mask, 0, sizeof(u32) * NUM_REC);
    memset(s_port, 0, sizeof(u32) * NUM_REC);
    memset(a_port, 0, sizeof(u32) * NUM_REC);
    read_all_data(fptr, path, s_ip, s_mask, s_port);

    struct rtInode *root = new_rtInode();
    root->cmpBit = 0;

    int test_num = test_upper_bound - test_lower_bound;

    for (int i = test_lower_bound; i < test_upper_bound; ++i) {
        // printf("Hello I am still here... %3d times\n",
        //        i - test_lower_bound);
        rt_Insert(root, s_ip[i], s_mask[i], s_port[i]);
    }

    //rt_dump((void *) root);

    // for cmp group:
    // * trie_node_t *tmp = pt_new_node(); tmp->port=0xFFFF;
    // trie_node_t *tmp;

    struct timespec
            time_start = {0, 0},
            time_end = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time_start);

    for (int i = test_lower_bound; i < test_upper_bound; ++i) {
        a_port[i] = rt_find_route(root, s_ip[i]);
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
