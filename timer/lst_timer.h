#ifndef MIN_HEAP
#define MIN_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>
#include <vector>

#define BUFFER_SIZE 64

class heap_timer;  /* 前向声明 */

struct client_data {
    sockaddr_in address;  // 客户端地址
    int sockfd;           // socket 文件描述符
    int No_;              // 对应的定时器在堆中的位置
};

/* 定时器类 */
class heap_timer {
public:
    heap_timer() {
        expire = time(NULL) + 5;
    }

public:
    time_t expire;                      // 定时器生效的绝对时间
    void (*cb_func) (client_data* );    // 定时器的回调函数
    client_data* user_data;             // 爆粗对应的HTTP连接信息
};

/* 时间堆类 */
class timer_heap {
public:
    timer_heap();
    timer_heap(int cap);
    timer_heap(const std::vector<heap_timer*> &init_array, int size, int capacity);

    void add_timer(heap_timer* timer); // 添加目标定时器
    void adjust_timer(int timer);      // 调整定时器在定时器容器中的位置
    void pop_timer();                  // 删除堆顶定时器
    void del_timer(int timer);         // 删除指定定时器
    void tick();                       // 定时任务处理函数
    const heap_timer* top();           // 返回堆顶定时器

private:
    std::vector<heap_timer*> array;  // 堆数组  下标从1开始
    int capacity;   // 容量
    int size;       // 包含定时器个数
    void siftdown(int k);
    void siftup(int k);
    int TIMELAG = 5;  // 每个连接的保活时间
};

/* 封装工具的类 */
class Utils {
public:
    Utils() = default;
    
    /* 设置最小超时单位 */
    void init(int timerslot);

    /* 将文件描述符设置为非阻塞 */
    int setnonblocking(int fd);

    /* 将内核事件表注册读事件; ET模式，选择开启EPOLLONESHOT */
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    /* 信号处理函数 */
    static void sig_handler(int sig);

    /* 设置信号的函数 */
    void addsig(int sig, void(hander)(int), bool restart = true);

    /* 定时处理任务, 重新定时以不断触发 SIGALRM 信号 */
    void timer_handler();

    void show_error(int connfd, const char* info);

public:
    static int *u_pipefd;  // 管道
    timer_heap heap;
    static int u_epollfd;  // epoll 例程
    int TIMELAG = 5;  // 每个连接的保活时间

};

void cb_func(client_data* user_data);

#endif