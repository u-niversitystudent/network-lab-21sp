# 高效 IP 路由查找实验<br/>实验报告

<!--实验报告: 模板不限, 内容包括但不限于实验题目、实验内容、实验流程、实验结果及分析-->

## 实验内容

基于所给的 `forwarding-table.txt` 数据集:

1. 实现最基本的前缀树查找方案;
2. 调研并实现某种 IP 前缀查找方案;
3. 检查 IP 前缀查找的正确性, 测量所实现方案的性能.

## 实验流程

### 实现: Trie

<!-- TODO: 实现最基本的 Trie 查找 -->

首先设计数据结构:

```c
typedef struct trie_node {
    struct trie_node *lchild, *rchild;
    u32 match, port;
} trie_node_t;
```

其中, `*lchild`, `*rchild` 分别是左孩子结点和右孩子结点, `match` 指示该结点是否为 Match Node, `port` 为结点存储的相应端口信息. 实际写代码的过程中, 为了方便 debug, 同时记录了 ip 信息, 但并不会在 build 或者 search 中用到, 因此最后删去了它.

实现建树操作:

```c
// 将输入的端口信息插入 trie 的对应位置
int pt_insert_node(trie_node_t *head, u32 ip, u32 mask, u32 port) {
    trie_node_t *current = head;
    // 在 for 循环中, 只前进到相应位置, 不做信息的更改
    for (int i = 0; i < mask; ++i) {
        if ((ip << i) & IP_HIGH) { // 从高位起第 i 位是 1
            if (IS_NULL(current->rchild)) {
                trie_node_t *nr = pt_new_node();
                current->rchild = nr;
            }
            current = current->rchild;
        } else { // 从高位起第 i 位是 0
            if (IS_NULL(current->lchild)) {
                trie_node_t *nl = pt_new_node();
                current->lchild = nl;
            }
            current = current->lchild;
        }
    }
    current->match = 1;
    current->port = port;
    return 0;
}

// 申请新结点
trie_node_t *pt_new_node() {
    trie_node_t *p = (trie_node_t *) malloc(sizeof(trie_node_t));
    memset(p, 0, sizeof(trie_node_t));
    return p;
}
```

假设已有一棵根为 `root` 的 Trie, 则使用以下的函数进行查询:

```c
// 输入 ip, 在以 `root` 为根的 Trie中进行查找
trie_node_t *pt_find_route(trie_node_t *root, u32 ip) {
    trie_node_t *found = NULL, *current = root;
    for (int i = 0; i < IP_LEN; ++i) {
        if (current == NULL) break; // 查找走到尽头

        if ((ip << i) & IP_HIGH) { // 从高位起第 i 位是 1
            if (IS_NULL(current->rchild)) break; // 查找走到尽头
            else {
                current = current->rchild;
                u32 cmp = (u32) ((ip | zero_64) << (i + 1));
                if (current->match == 1 && cmp == 0) found = current;
            }
        } else { // 从高位起第 i 位是 0
            if (IS_NULL(current->lchild)) break; // 查找走到尽头
            else {
                current = current->lchild;
                u32 cmp = (u32) ((ip | zero_64) << (i + 1));
                if (current->match == 1 && cmp == 0) found = current;
            }
        }
    }

    return found;
}
```

查找结束后, 打印单次查找平均用时和 diff 次数.

```c
// trie 实例中: 打印用时和diff次数
    printf("--------\n"
           "Summary for %d times' lookups:\n"
           "diff:\t %d times.\n"
           "time:\t %.5lf ns per lookup.\n",
           NUM_REC, count, interval);
```

在命令行参数中有 `-r` 时, 打印全部查找结果:

```c
// 主函数中: 打印查找结果
if (if_print_result) {
        // Result
        printf("--------\nResult:\n");
        for (int i = 0; i < NUM_REC; ++i) {
            if (s_port[i] == a_port[i]) printf("[same]");
            else printf("[diff]");
            printf("DATA: port=%d when ip="IP_FMT" mask=%d \n"
                   "     ROUTE: port=%d\n",
                   s_port[i], LE_IP_FMT_STR(s_ip[i]), s_mask[i],
                   a_port[i]);
        }
    }
```

这样在保证性能测试结果正确性的情况下, 能更好地检验数据集重叠冲突的情况下是否能正确查找.

此外, 为了实现这个实验, 我还实现了以下函数:

```c
// 格式转换, in fmt.h
u32 ip_str_to_u32(char *input);
u32 mask_str_to_u32(char *input);
u32 port_str_to_u32(char *input);
// 数据集读取, in fread.h
void read_all_data(FILE * fptr, char *path, u32 *ip, u32 *mask,u32 *port);
```

格式转换函数能从所给数据集中的单行字符串提取出正确的信息; 数据集读取从文件中读取字符串, 解析字符串, 并将它们存入全局数组中, 以便各函数调用. 由于这些细琐的函数并不是算法实现的重点, 故不再赘述.

### 调研: PopTrie

<!-- TODO: 介绍某种升级方案 -->

### 实现: PopTrie

<!-- !TODO: 实现某种升级方案 -->

## 实验结果与分析

### Trie

考虑到函数设计中存在一处辅助的赋值操作, 可能影响计时准确性, 设置一个 "无路由" 的对照组, 记录完成其他操作的单次用时:

```
zxc@XZheng:/mnt/c/ZHENG/code-local-repo/labs-network/lab-10-lookup/prj-10$ ./cmake-build-debug/prj_10
Exec Trie function...
--------
Summary for 697882 times' lookups:
diff:    697882 times.
time:    2.58425 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    697882 times.
time:    2.58468 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    697882 times.
time:    2.58554 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    697882 times.
time:    2.59671 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    697882 times.
time:    2.79388 ns per lookup.
```

计算对照组平均用时:

$$
(2.58425+2.58468+2.58554+2.59671+2.79388) \div 5 = 2.62901
$$

记录实验组的单次操作用时:

```
zxc@XZheng:/mnt/c/ZHENG/code-local-repo/labs-network/lab-10-lookup/prj-10$ ./cmake-build-debug/prj_10
Exec Trie function...
--------
Summary for 697882 times' lookups:
diff:    2646 times.
time:    123.16595 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    2646 times.
time:    124.03902 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    2646 times.
time:    123.34334 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    2646 times.
time:    123.07381 ns per lookup.
--------
Summary for 697882 times' lookups:
diff:    2646 times.
time:    123.33819 ns per lookup.
```

计算实验组平均用时:

$$
(123.16595+124.03902+123.34334+123.07381+123.33819) \div 5 \approx 123.39206
$$

得到单次路由查找平均用时为 123.39206 - 2.62901 = **120.76305** ns.

### PopTrie

<!-- ## 思考题 -->

<!-- 请将思考/调研结果写到实验报告中 -->

<!-- ## 实验反思 -->

<!-- ### 调试中出现过的问题 -->

## 参考资料

<!--脚注-->
