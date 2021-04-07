# readme for current prj

标有 [※] 的为**有改动**的文件或**新增**文件.

- 源程序和头文件
  - include/: 源程序用到的头文件
  - main.c: 主程序
  - device_internal.c: hub 程序用到的其他库函数
  - broadcast.c: 节点广播函数 [※]
- 脚本
  - Makefile: 编译源程序使用的 makefile 文件
  - scripts/: 运行辅助脚本
  - three_nodes_bw.py: 建立虚拟网络环境
  - loop_nodes_bw.py: 建立环形拓扑的虚拟网络环境 [※]
  - measure.sh: iperf 测量客户端脚本 [※]
- 其他
  - hub: 编译得到的 hub 可执行程序 [※]
  - hub-reference/hub-reference.32: hub 可执行程序
