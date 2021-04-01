# Socket 应用编程实验<br/>实验报告

<!--实验报告：模板不限, 内容包括但不限于实验题目、实验内容、实验流程、实验结果及分析-->

## 实验内容

1. 使用 C 语言实现最简单的 HTTP 服务器与 HTTP 客户端, 要求支持 HTTP GET 方法.
2. 使用 python 脚本 `topo.py` 在本地建立虚拟网络环境, 对自己编写的 HTTP 服务器/ HTTP 客户端进行功能测试.

### 实现要求

#### HTTP 服务器

HTTP 服务器的基本功能:

1. 监听指定端口 (默认 `80` ), 能接收来自客户端的 HTTP GET 请求;
2. 对收到的 HTTP 请求能回复相应的 HTTP 应答;
   - 所请求的文件存在于服务器程序当前目录中, 则返回 `HTTP 200 OK` 和相应文件, 否则返回 `HTTP 404 File Not Found`;
3. 支持多个客户端进程分别请求获取文件, 支持客户端连续多次请求获取文件.

#### HTTP 客户端

HTTP 客户端的基本功能:

1. 可以通过命令行参数和程序内部机制指定需要请求的文件 URL, 并向 HTTP 服务器发送 HTTP GET 请求;
2. 能连续多次向服务器请求获取文件.

### 测试要求

#### 测试对象

1. 使用自己实现的 HTTP 客户端向自己实现的 HTTP 服务器请求文件;
2. 使用 `wget` 命令向自己实现的 HTTP 服务器请求文件;
3. 使用自己实现的 HTTP 客户端向用 `python -m SimpleHTTPServer 80`启动的 HTTP 服务器请求文件.

#### 测试内容

1. 分别测试 "请求获取存在于服务器程序当前目录中的文件" 和 "请求不存在的文件" 的情形;
2. 测试客户端连续多次获取文件的情况;
3. 测试同时启动多个客户端进程分别获取文件的情况.

## 实验流程

### HTTP 客户端的实现

#### 主动建立连接

1. 客户端使用 `socket` 函数建立套接字 `sock`, 根据返回值判定是否建立成功, 若成功则程序继续运行, 失败则在打印报错信息后主动退出程序;
2. 客户端使用 `connect` 函数主动建立连接, 根据返回值判定是否建立成功, 若成功则程序继续运行, 失败则在打印报错信息后主动退出程序.

代码实现如下:

```c
    // 建立套接字文件描述符 'sock'
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
      printf("SOCKET FAILED\n");
      return -1;
    }
    printf("SOCKET CREATED\n");

    // 主动建立连接
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr("10.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(SPORT);
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
      perror("CONNECT FAILED: ");
      return 1;
    }
    printf("CONNECTED!!!\n");
```

#### 发送 HTTP 请求

如果 HTTP 客户端想要向 HTTP 服务器请求获取文件, 首先需要编码 HTTP 请求使其符合 HTTP 协议, 其次需要通过已建立的连接发送 HTTP 请求.

根据实验要求, 用户可以通过命令行参数或程序内输入提供文件的 URL, 因此设置变量 `f_in` 来指示是否使用命令行参数中的 URL:

```c
  // 检查命令行参数, 设置正确的 'f_in' 标志
  int f_in = 0;
  if (argc > 2)
    perror("Too many args");
  else if (argc > 1) {
    f_in = 1; // If input a dest, fin = 1
    memcpy(url, argv[1], (strlen(argv[1]) + 1));
  }

  // 以下的 'while' 是连续请求文件的部分
  while(1){
    /* many other things*/

    if (!f_in) {                 // f_in==0, 不使用命令行参数
      printf("Enter url (host+path): (type 'q' to quit)\n");
      scanf("%s", url);             // 从用户输入中读取 URL
      if (strcmp(url, "q") == 0)    // 在用户输入 'q' 时退出程序
        break;
    } else                       // f_in!=0, 使用命令行参数
      f_in = 0;                     // 只使用 1 次, 因而立刻置 0

    // 从 'url' 字符串中解码,
    // 得到目标主机地址 'dhost' 和目标文件(路径) 'dpath'
    decode_input(url, dhost, dpath);

    // 根据以上信息, 编码格式正确的 HTTP GET 请求,
    // 并存入 'usr_req'
    char *method = "GET";    // method
    char *ptcl = "HTTP/1.1"; // protocol
    char *hseg = "HOST:";    // head segname+":"
    sprintf(usr_req,
            "%s %s %s\r\n%s%s\r\n\r\n",
            method, dpath,
            ptcl, hseg, url);

    /* many other things*/
  }
```

HTTP GET 请求编码完成后, 就可以借助套接字和已经建立的连接向服务器发送 HTTP GET 请求了. 使用 `send` 函数时, 根据返回值判定是否建立成功, 若成功则程序继续运行, 失败则在打印报错信息后主动退出程序.

代码实现如下:

```c
    if (send(sock, usr_req, strlen(usr_req), 0) < 0) {
      printf("SEND FAILED\n"); return 1;
    }
    printf("SENT!!!\n");
```

#### 接收并解析 HTTP 应答

设置文件接收时的本地存储路径为 `out/<filename>`:

```c
    // 设置 GET 文件的存储路径 'out/filename'
    char *path_out = "out/"; // output path
    char dest[NAME_SZ];
    memset(dest, 0, NAME_SZ);
    strcat(dest, path_out); strcat(dest, dpath);
```

使用 `recv` 函数接收 HTTP 应答报文并存入 `svr_ans`, 打印报文内容, 根据报文中的 HTTP 请求状态决定下一步的行动:

- HTTP 请求状态为 `HTTP 200 OK`: 跳过报头, 存储文件到设置的路径
- HTTP 请求状态为 `HTTP 404 File Not Found` : 打印 `404 File Not Found` 提示

```c
    // 接收 HTTP 应答
    int len_ans = 0;
    memset(svr_ans, 0, sizeof(svr_ans));
    while ((len_ans = recv(sock, svr_ans, MSG_SZ, 0)) != 0) {
      if (len_ans < 0) {
        printf("RECV FAILED!\n"); break;
      }
      // 打印 HTTP 报文
      printf("%s\n", svr_ans);

      // 解码 HTTP 应答报文中的 HTTP 请求状态
      char state[4];
      sscanf(svr_ans, "%*[^ ] %[^ ]", state);
      if (strcmp(state, "404") != 0) { // state: 200 OK
        printf("%s OK!\n", state);
        // 跳过应答报文的报头
        int start;
        for (start = 0; start < len_ans; start++) {
          if (svr_ans[start]     == '\r' && svr_ans[start + 1] == '\n' &&
              svr_ans[start + 2] == '\r' && svr_ans[start + 3] == '\n') {
            start += 4; break;
          }
        }
        // 读报文的文件传输部分, 存储到本地
        FILE *fp = fopen(dest, "a+");
        unsigned long wret =
            fwrite(&svr_ans[start], sizeof(char), len_ans - start, fp);
        if (wret < len_ans - start) {
          printf("FWRITE FAILED!\n"); break;
        }
        printf("FWRITE FINISHED!\n"); fclose(fp); break;
      } else { // state: 404 File Not Found
        printf("404 File Not Found.\n"); break;
      }
    }
```

该段程序在循环 `while(1)` 中执行, 故在该段程序结束运行后, 用户可以发送下一个请求或者选择退出程序.

### HTTP 服务器的实现

#### 准备接收请求

1. 根据命令行参数的情况, 设置服务器的监听端口;
   - 若 `argc==1` , 则使用默认端口 `80`, 否则使用 `argv[1]` 位置指定的端口 (不考虑非法输入);
2. 建立套接字, 并绑定监听端口 (`socket` 函数和 `bind` 函数);
3. 开始监听该监听端口 (`listen` 函数).

代码实现如下:

```c
  // 根据命令行参数的情况, 设置监听端口
  int port = (argc < 2) ? SPORT : atoi(argv[1]);

  // 建立套接字
  int s; struct sockaddr_in server, client;
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Create socket failed"); return -1;
  }
  printf("Socket created\n");

  // 将套接字与监听端口绑定
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Bind failed"); return -1;
  }
  printf("Bind done\n");

  // 开始监听
  listen(s, MAX_SESSION);
  printf("Waiting for incoming connections...\n");
```

#### 接收: 多路并发的 HTTP GET 请求

1. 使用 `accept` 函数, 接收来自客户端的连接;
2. 对于每一个成功建立的连接, 都新建一个 `receiver` 线程, 用来处理 HTTP 请求.

```c
  // 接收 & 处理来自 HTTP 客户端的 HTTP GET 请求
  while (1) {
    pthread_t thread;
    int *cs; cs = (int *)malloc(sizeof(int));
    int c = sizeof(struct sockaddr_in);
    // 接收来自 HTTP 客户端的连接
    if ((*cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
      perror("Accept failed"); return -1;
    }
    printf("Connection from %s was accepted\n", inet_ntoa(client.sin_addr));
    // 建立新的 'receiver' 线程以处理刚收到的连接
    if (pthread_create(&thread, NULL, receiver, cs) != 0) {
      perror("pthread_create failed: "); return -1;
    }
  }
```

#### `receiver` 函数: 解析 HTTP GET 请求, 编码并发送 HTTP 应答

因为 `receiver` 线程需要解析接收的请求, 编码并发送 HTTP 应答, 所以初始化了若干个缓冲区, 用来存放传输的数据和路径信息, 每次循环开始时置 `0` [^1] 清空.

[^1]: 即 `\0` 的 ASCII 值.

```c
  // 初始化缓冲区
  char clt_req[MSG_SZ], ans[MSG_SZ], file[MSG_SZ], dpath[NAME_SZ];
  int len = 0;
  int sock = *((int *)cs);

  while (1) {
    // 清空缓冲区
    memset(clt_req, 0, MSG_SZ); memset(ans, 0, MSG_SZ);
    memset(file, 0, MSG_SZ); memset(dpath, 0, NAME_SZ);

    /* many other things */
  }
```

使用 `recv` 函数接收来自客户端的 HTTP GET 请求:

```c
    // RECV req; SEND ans & data
    len = recv(sock, clt_req, sizeof(clt_req), 0);
```

根据 `len` 的长度, 判断请求有效与否:

- `len>0`: 请求有效, 准备解析请求做出应答
  - 解析得到文件名, 判断文件是否存在;
  - 根据文件的存在情况, 编码对应的 HTTP 应答报文;
- 其他情况: 请求无效, 即请求接收失败或客户端断连, 此时应该
  - 打印报错信息;
  - 关闭套接字, 退出线程, 等待下一个请求.

```c
    if (len > 0) /* recv ok */ {
      // 匹配文件名 (文件路径)
      sscanf(clt_req, "%*s /%[^ ]", dpath);
      printf("%s\n", dpath);

      // 建立文件流读写指针
      FILE *fp = fopen(dpath, "r");
      // 判断文件是否存在, 进一步处理
      if (fp) /* 文件存在 / 200 OK */
      {
        printf("Start sending %s to the client.\n", dpath);
        unsigned long rret = 0;
        while ((rret = fread(file, sizeof(char), MSG_SZ, fp)) > 0) {
          sprintf(ans, "%s%lu\r\n\r\n%s", ok, rret, file);
          printf("%s\n", ans);
          if (send(sock, ans, strlen(ans), 0) < 0) {
            printf("File send failed\n"); break;
          }
        }
        fclose(fp); printf("File transfer finished\n");
      }
      else /* 文件不存在 / 404 NOT FOUND*/
      {
        printf("File %s not found\n", dpath);
        strcpy(ans, fail);
        if (send(sock, ans, strlen(ans), 0) < 0) {
          printf("Answer send failed\n"); break;
        }
      }
    }
    else /* 客户端退出/请求接收错误 */
    {
      if (len == 0) // 客户端退出 (正常)
        printf("Client disconnected\n");
      else          // 请求接收错误 (出错)
        perror("Recv failed: ");

      // 关闭套接字
      close(sock); free(cs);
      // 线程退出
      pthread_exit(NULL);
    }
  }
```

### 调试中遇到的问题

运行 HTTP 服务器程序时, 程序报错 `Address already in use` 后退出.

**解决方案** (`.docx` 帮助文档中的第 3 条): 设置端口重用.

代码实现如下:

```c
  // 设置端口重用
  int yes = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    perror("Setsockopt error: ");
    return -1;
  }
```

## 实验结果与分析

### 测试流程介绍

使用自己编写的 `shell` 脚本 `svr.sh` , 删除之前的临时文件, 为测试设置下载目录和示例文件, 脚本内容如下:

```shell
#!/bin/bash
# 'svr.sh'
echo "remove temp files..."
rm -rf out/
mkdir out/
rm *.txt
rm test*
echo "add test file 'test.txt' via echo"
echo "Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit..." >> test.txt
echo "process finished"
```

使用 `sudo python topo.py` 建立虚拟网络环境, 并进行对应的测试.

### 测试结果: 3 对服务器/客户端程序

为方便起见, 下称自己实现的 `http_client` 和 `http_server` 为 `my client` 和 `my server`.

#### 客户端 `my client` 与服务器 `my server`

<img src="lab-02.assets/image-20210401225122516.png" alt="image-20210401225122516" style="zoom:50%;" />

#### 客户端 `wget` 命令与服务器 `my server`

<img src="lab-02.assets/image-20210401225358421.png" alt="image-20210401225358421" style="zoom:33%;" />

#### 客户端 `my client` 与服务器 `python -m SimpleHTTPServer`

<img src="lab-02.assets/image-20210401230009372.png" alt="image-20210401230009372" style="zoom:33%;" />

#### 连续/混合发送请求

简并测试需求, 下面给出单独测试两个程序的结果.

1. 使用 `python` 的 `SimpleHTTPServer`, 交替使用 `my client` 和 `wget` 获取文件 (存在/不存在), 结果如下:

<img src="lab-02.assets/image-20210401230323147.png" alt="image-20210401230323147" style="zoom:33%;" />

2. 使用 `my server` , 交替使用 `my client` 和 `wget` 获取文件 (存在/不存在), 结果如下:

<img src="lab-02.assets/image-20210401230754708.png" alt="image-20210401230754708" style="zoom:33%;" />

### 实验结论

通过测试, 可知 "实验目标" 中的功能均已实现, 该简易 HTTP 服务器程序/ HTTP 客户端程序通过测试.

## 参考资料

1. [HTTP 消息 - HTTP | MDN](https://developer.mozilla.org/zh-CN/docs/Web/HTTP/Messages)
2. [socket 编程的端口和地址复用\_魏波-CSDN 认证博客专家-CSDN 博客](https://blog.csdn.net/weibo1230123/article/details/79978745)
3. [c, control reaches end of non-void function in c - Stack Overflow](https://stackoverflow.com/questions/7007231/c-control-reaches-end-of-non-void-function-in-c)
4. [C 语言实现 HTTP 的 GET 和 POST 请求 - 简书](https://www.jianshu.com/p/867632980b65)
5. [程序员都该懂点 HTTP - 掘金](https://juejin.cn/post/6844903511633707021)
