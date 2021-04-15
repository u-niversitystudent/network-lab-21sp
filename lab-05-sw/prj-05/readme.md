# readme for current prj

标有 [※] 的为**有改动**的文件或**新增**文件.

## 源程序和头文件

- main.c: 主程序, 增加了广播/单播转发判断等 [※]
- mac.c: 交换机转发表操作函数 [※]
- broadcast.c: lab 04 中实现的节点广播函数
- include/: 自定义头文件库
- device_internal.c: switch 程序用到的其他库函数

## 脚本

- measure.sh: iperf 测量客户端脚本 [※]
- Makefile: 编译源程序使用的 makefile 文件
- scripts/: 运行辅助脚本
- three_nodes_bw.py: 建立虚拟网络环境

## 其他

- \*.o: 对象文件 (可执行文件)
- 无后缀名的文件: 可执行文件
