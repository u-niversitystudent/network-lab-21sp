# 高效 IP 路由查找实验<br/>实验报告

<!--实验报告: 模板不限, 内容包括但不限于实验题目、实验内容、实验流程、实验结果及分析-->

## 实验内容

基于所给的 `forwarding-table.txt` 数据集:

1. 实现最基本的前缀树查找方案;
2. 调研并实现某种 IP 前缀查找方案;
3. 检查 IP 前缀查找的正确性, 测量所实现方案的性能.

## 实验流程

### 实现: Trie

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

格式转换函数能从所给数据集中的单行字符串提取出正确的信息; 数据集读取从文件中读取字符串, 解析字符串, 并将它们存入全局数组中, 以便各函数调用. 由于这些细节处的函数并不是算法实现的重点, 故不在实验报告中细述.

### 调研: 基于 PATRICIA tree 提出的路由匹配算法

<!-- TODO: 介绍某种升级方案 -->

我选择的算法是论文 "A Tree-Based Packet Routing Table for Berkeley UNIX" 中实现的压缩前缀树算法, 著名的 Linux 发行版 FreeBSD 使用这一算法作为其路由查找算法.

下图是从论文中摘录的一张图, 它说明了树的结构:

![routing_example](readme.assets/routing_example.png)

这种 Trie 涉及两种结点: 中间结点, 叶子结点. 中间结点存储准备匹配的 bit 是第几位 (0~31), 叶子结点存储一对 port-mask.

搜索算法大致如下:

```c
// TopNode 为 Trie 的头结点,
// current 为当前结点
current = &TopNode;
void Search(Node *current, u32 ip) {
	while (current) { // 向下循环求解
		if (IsLeaf(current)){
            // 当前为叶结点,
            //按照ip匹配情况返回port值或error值
			if (IsMatched(current, ip))
                return current->port;
			else
                return PORT_ERROR;
		}
		else {
            // 当前结点为中间结点,
            // 根据被抽取bit的值选择前进到左孩子or右孩子
			if (ExtractBit(current, ip) != 0)
                current = current->rightChild;
			else
                current = current->leftChild;
		}
	}
}
```

Reduced trie 建立的算法如下:

```
输入: 插入信息, 树的根结点

步骤:
    1. 将 current 指向树的根结点;
    2. 重复下列步骤 (~);
    3. 当 current 为空指针时, 退出程序;
    4. 当 current 为中间结点时, 结合孩子结点的情况, 根据需要构造新的中间结点, 然后 current 向相应方向前进一个结点;
    5. 当 current 为叶子结点时, 结合叶子结点的信息, 构造适当的叶子结点和中间结点, 插入树中, 退出程序.
```

由于具体算法实现比较繁杂, 列举代码意义不大, 故在此处略去, 完整实现见 `reducedTrie.c` 和 `reducedTrie.h`.

### Program Usage

该程序使用命令行参数决定算法和辅助功能的开关:

- 默认使用 normal trie 算法, 若有 `-t` 参数则使用 reduced trie 算法;
- 如需打印, 需要添加 `-r` 参数.

此外, 脚本 `exec.sh` 支持执行两种算法各 5 次, 并将结果存入对应的 `*.log` 文件中.

## 实验结果与分析

### 对正确性的说明

实验中, 采用 "查询结果" 与 "原始数据集" 的 diff 结果作为正确性测试的参考.

1. 两份记录一致, 说明查询无误;
2. 两份记录不一致, 检查是否为 "匹配到更长前缀的条目" 的情况.

两种算法均得到在近 700,000 个条目中有 2646 处不一致, 抽样检查显示结果正确, 故有理由认为已实现的两种算法都能依据静态数据集得出正确结果.

### Normal Trie

#### 内存

因为基础实现未压缩点, 故一共使用了 2^32-1 个结点, 每个结点至少需要以下信息:

- 左孩子指针, 右孩子指针;
- match/internal node 标识;
- 端口号;

中间结点和匹配结点均使用同一数据结构, 一共申请的内存大小大约为

$$
(2^{32}-1) \times (8\times 2 + 4+4)\approx 96.00 {\rm GiB}.
$$

#### 耗时

考虑到函数设计中存在一处辅助的赋值操作, 可能影响计时准确性, 设置一个 "无路由" 的对照组, 记录完成其他操作的单次用时:

```
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 697882 times.
time:	 1.46486 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 697882 times.
time:	 1.64039 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 697882 times.
time:	 1.35768 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 697882 times.
time:	 2.05952 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 697882 times.
time:	 4.41364 ns per lookup.
```

计算知对照组平均用时约为 2.19ns.

记录实验组的单次操作用时:

```
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 99.95931 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 99.54090 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 99.54935 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 103.28738 ns per lookup.
Executing function normal trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 115.13838 ns per lookup.
```

计算知实验组平均用时约为 103.50ns.

得到单次路由查找平均用时为 103.50 - 2.19 = **101.31**ns.

### Reduced Trie

#### 内存

<!--TODO-->

该实现有两种类型:

(1) 每个中间结点至少需要以下信息:

- match/internal node 标识;
- 左孩子指针, 右孩子指针, 父母指针;
- 当前步骤应比较的 bit 位置.

由于大约平均有 $\log_2 697882\approx 19.41$ 层结点, 故有大约 524287 个这样的结点.

(2) 每个叶子结点至少需要以下信息:

- match/internal node 标识;
- 父母指针;
- 掩码, 端口, ip 号.

共有 697882 个这样的结点.

中间结点和匹配结点共申请内存大小大约为

$$
(4+24+4)\times 524287 + (4+8+12)\times 697882 \approx 31.97 {\rm MiB}.
$$

#### 耗时

对照组:

```
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 618396 times.
time:	 2.01940 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 618396 times.
time:	 2.15738 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 618396 times.
time:	 2.08173 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 618396 times.
time:	 5.32626 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 618396 times.
time:	 3.91900 ns per lookup.
```

计算知对照组平均用时约为 3.10ns.

记录实验组的单次操作用时:

```
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 159.63028 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 159.41606 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 159.57827 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 161.55998 ns per lookup.
Executing function reduced trie
--------
Summary for 697882 times' lookups:
diff:	 2646 times.
time:	 165.60708 ns per lookup.
```

计算知实验组平均用时约为 161.16ns.

得到单次路由查找平均用时为 161.16 - 3.10 = **158.06**ns.

### 比较: Basic Trie vs Reduced Trie

从上面的数据可知, Reduced Trie 虽然在耗时上比 Normal Trie 多了 56%, 但却只有 Normal Trie 申请内存的 0.032%, 时间换空间的性价比非常高, 假以适当的调优, 一定能够称为不错的策略.

<!-- ## 思考题 -->

<!-- 请将思考/调研结果写到实验报告中 -->

## 实验反思

### 调试中出现过的问题

一开始性能方面没有做很好的优化, 实测使用 `size` 为 10,000 的数据集需要将近 10 分钟才能将 Trie 建完. 假设建 Trie 耗时线性增长, 使用 `size` 接近 700,000 的原数据集需要将近 12 小时, 并且内存也不足以让程序运行完成.

最终, 使用 GDB 跟踪, 发现性能劣势的主要原因是一个重要的函数使用了递归形式. 将递归修改为迭代之后, 建树的时间趋于正常值.

### 可能的优化方向

如果改为**多比特前缀**, 或者**加入直接索引** "化树为森林", 都有可能减小索引/内存开销, 进而提高性能/加快查找速度.

## 参考资料

1. [Sklower K. A tree-based packet routing table for Berkeley unix[C]//USENIX Winter. 1991, 1991: 93-99.](https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.121.9223&rep=rep1&type=pdf)
2. [data structures - What is the difference between radix trees and Patricia tries? - Computer Science Stack Exchange](https://cs.stackexchange.com/questions/63048/what-is-the-difference-between-radix-trees-and-patricia-tries)

<!--脚注-->
