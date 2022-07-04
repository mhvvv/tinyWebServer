# tinyWebServer

代码存在一点细节问题，正在快马加鞭修改中....  


## ✅ 项目简介
一个运行于Linux环境下, 用 C/C++ 实现的轻量级HTTP服务器, 用于学习网络编程。

项目实现了以下功能:
* 使用 **epoll**、**线程池**、和 **socket** 实现了Proactor 和 Reactor 模式的并发模型
* 使用 **状态机** 解析HTTP请求报文, 包括 **GET请求** 和 **POST请求**
* 使用 **数据库连接池** 实现注册功能
* 使用 **定时器** 和 **SIGALRM信号** 检查并关闭处于非活跃状态的HTTP连接
* 使用 **互斥量** 和 **信号量** 实现线程同步
* 实现 **同步/异步日志系统**, 记录服务器运行状态

在原项目基础上的改进:
* 使用 **小根堆** 替代链表重新实现了定时器容器

值得思考的几个点:
* 什么是数据库连接池用到的单例模式和RAII模式
* 如何在定时器容器中快速定位到一个TCP连接对应的定时器
* 如何减少数据在内核空间和用户空间之间的拷贝次数

## ✅ 一些概念

先不解释，都写在这万一面试的时候没东西说就坏了，嘿嘿嘿 😆

- **非阻塞I/O and 阻塞I/O：** 调用函数期待处理某事件，而该事件未发生时，一直等待 or 立即返回 (通常会设置errno
- **I/O复用：** 一个进程或线程同时监听多个文件描述符上是否有事件发生，若有则做出相应处理
- **同步I/O and 异步I/O：** 同步I/O指内核向应用程序通知的是就绪事件，比如某个套接字上有数据可读时，内核通知用户进程，要求用户进程自行执行I/O操作; 异步I/O是指内核向应用程序通知的是完成事件，即将数据读完后才通知用户进程，由内核完成I/O操作 
- **事件处理模式 Reactor and Proactor:** 
    - **Reactor：** 主线程只负责监听文件描述符上是否有事件发生，有事件发生时通知工作线程进行处理，即新连接的建立、数据的读写以及客户端请求的处理均由工作线程完成。通常由同步I/O实现
    - **Proactor：** 主线程负责接受新的客户端连接、读写I/O等操作，工作线程仅负责逻辑处理，即主线程将数据读取完成后通知工作线程直接进行逻辑处理。一般用异步I/O实现
- **同步I/O模拟Proactor模式**
    1. 主线程创建epoll例程，注册服务器socket读就绪事件以及已经建立连接用于同客户端通信的套接字，等待客户端请求连接建立请求以及用于通信的套接字的读就绪写就绪事件
    2. 当epoll例程检测到有套接字或者说文件描述符出现可读事件，判断是服务器socket还是用于与客户端通信的socket
        - 服务器socket：主线程建立新连接，并将新创建的socket注册到epoll例程
        - 通信socket：
            1. 主线程从socket循环读取数据，直到没有更多数据可读。然后将读取到的数据封装成一个请求对象插入到请求队列
            2. 线程池中的一个工作线程被唤醒，被唤醒的工作线程到请求队列中取一个请求并处理，处理完成后向epoll例程注册对应的通信socket写就绪事件**（所有工作线程睡眠在请求队列上，当有任务到来时，通过竞争（如互斥锁）获得任务的接管权）**
    1. 当epoll例程检测到socket可写事件后，通知主线程发送数据

## ✅ 用到的设计思想

**RAII**, 全称"Resource Acquisition is Initialization", 中文翻译为: **资源获取即初始化**.


资源的使用一般分为三个阶段: 1. 获取资源; 2. 使用资源; 3. 释放资源. 但释放资源容易被程序员忘掉. 

RAII机制将资源的生命周期与类对象的生命周期绑定，使用类的构造函数获取资源，使用类的析构函数释放资源，当需要资源时，只需要实例化一个类对象即可。 

由于C++语言的特性，在创建对象时会自动调用构造函数，销毁对象时会自动调用析构函数，且对象在离开其作用域时会自动调用析构函数销毁自己.

**独占所有权的智能指针 unique_ptr** 是RAII 的一个很好的例子.


## ✅ 整体流程

#### 同步I/O模拟Proactor模式
1. 主线程创建epoll例程，注册服务器socket读就绪事件以及已经建立连接用于同客户端通信的套接字，等待客户端请求连接建立请求以及用于通信的套接字的读就绪写就绪事件
2. 当epoll例程检测到有套接字或者说文件描述符出现可读事件，判断是服务器socket还是用于与客户端通信的socket
   - 服务器socket：主线程建立新连接，并将新创建的socket注册到epoll例程
   - 通信socket：
      1. 主线程从socket循环读取数据，直到没有更多数据可读。然后将读取到的数据封装成一个请求对象插入到请求队列
      2. 线程池中的一个工作线程被唤醒，被唤醒的工作线程到请求队列中取一个请求并处理，处理完成后向epoll例程注册对应的通信socket写就绪事件**（所有工作线程睡眠在请求队列上，当有任务到来时，通过竞争（如互斥锁）获得任务的接管权）**
3. 当epoll例程检测到socket可写事件后，通知主线程发送数据


#### HTTP 处理流程
1. 主线程监听到用于与HTTP通信的套接字上有事件发生时，主线程创建一个HTTP类对象，将收到的数据全部读入该对象的缓冲区（char[])，HTTP类对象同时会记录标识对应HTTP连接的套接字。然后将该HTTP对象插入到请求队列，然后用对对应的信号量执行post操作，唤醒阻塞队列上的一个工作线程，对本次收到的请求进行处理。 这里本质上是一个生产者消费者模型。

2. 工作线程从任务队列中取出一个任务进行处理, 首先对存储在HTTP对象读buffer里的请求内容进行解析, 这里使用主状态机和从状态机进行解析(状态机本质上就是 一系列条件判断语句和字符串查询某字符位置的操作函数)

具体的, HTTP请求报文由请求行、请求头、空行、请求体组成，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n。因此，可以通过查找\r\n将报文拆解成单独的行进行解析。

主状态机调用从状态机循环读取HTTP请求报文的每一行返回给主状态机进行解析，主状态机需要解析的内容为：
* 请求行 GET / POST， URL， HTTP1.1
* 请求头中请求体长度字段，这里我们发送的GET请求不包括请求体，请求体长度为0，而POST请求用于注册和登录，将账号和密码放在请求体中，因此请求体长度不为0.
* 当解析到空行时，根据是GET请求还是POST请求决定是否继续解析请求体（可以通过GET/POST判断，也可以通过请求体长度字段判断)
解析结果有如下三种：
* 请求登录或注册页面，此种情况下直接将响应报文行、响应头写到一个字符串数组里，将响应体，也就是请求的资源通过`mmap`函数取到在内存中的地址；然后将两部分内容通过`iovec`结构体和`writev` 函数拷贝到服务器socket写缓冲区，让后注册可写事件，通知主进程发送数据。使用`mmap`函数是为了实现零拷贝，提高效率
* 请求登录. 先在本地用户表中进行用户名和密码的匹配，然后使用上述方法放回登录成功或失败页面
* 请求注册. 现在本地用户表中判断用户名是否已存在，不存在则从数据库连接池中取一个连接将注册信息插入到数据库，同时更新本地用户表。然后返回注册成功或失败页面。


#### 日志流程

同步日志直接写入文件

异步日志先写入缓冲队列, 然后单独创建线程处理缓冲队列

## ✅ 遇到的问题 / 细节问题

### 1. CTRL+C 中止服务器后，不能立刻重启服务器

**原因分析：**

这是因为TCP正常断开连接要经历四挥手过程，主动断开连接的一方会经历TIME-WAIT阶段.

**解决方法：**
1. 复用处于 TIME-WAIT状态的 socket 为新的连接所用
    * 用户态: 使用 `SO_REUSEADDR` 设置套接字
        ```C++
        setsockopt(serv_sock, SOL_SOCLET, SO_REUSERADD, ...)
        ```
    * 内核选项：tcp_tw_reuse
2. 使用 SO_LINGER 设置套接字
    ```C++
    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    setsockopt(s, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

    用结构体linger实例化一个对象中第一个成员的值设为非0, 第二值设为0. 然后用该对象通过 setsockopt() 设置套接字, 那么调用 close() 函数后, 
    会直接发送 RST 标志给对方, 该TCP连接将不进行四握手, 直接关闭. 
    ```
### 2. writv + ivoc + mmap

### 3. LT ONESHOT



## 致谢
[qinguoyi / TinyWebServer](https://github.com/qinguoyi/TinyWebServer)

《TCP/IP 网络编程》尹圣雨
