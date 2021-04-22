# readme for current prj

> [※] = modified.
>
> 仅对重要文件作标注, 框架文件一般不标注.

## 源程序和头文件 (C 语言项目)

- main.c: 主程序, handle_packet 整合了数据包转发和 config 处理 [※]
- mac.c: lab 05 中完成的交换机运行机制 [※]
- broadcast.c: lab 04 中实现的节点广播函数, 添加了关于 Alternative Port 的发包判断 [※]
- include/: 项目头文件库
- Makefile: 编译源程序使用的 makefile 文件, 添加了源程序 mac.c, boardcast.c

## 脚本

- test_and_dump.sh: 生成树拓扑测量脚本, 可指定参数, 自动化执行 [※]
- four_node_ring.py: 4 节点环状拓扑构建
- six_node.py: 6 节点扩展脚本, 用于测试交换机结合生成树的效果
- new_topo.py: 9 节点扩展脚本, 验证生成树算法的自定拓扑构建 (中等程度的 benchmark)

## 其他

- \*.o: 对象文件 (可执行文件)
- 无后缀名的文件: 可执行文件
