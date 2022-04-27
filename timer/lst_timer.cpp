#include "lst_timer.h"
#include "http_conn.h"

timer_heap::timer_heap() {
    capacity = 0;
    size = 0;
}

timer_heap::timer_heap(int cap) : capacity(cap), size(0) {
    array.resize(capacity + 1, NULL);
}
timer_heap::timer_heap(const std::vector<heap_timer*> &init_array, int size, int capacity) {
    if(capacity < size) {
        throw std::exception();
    }
    array = init_array;
    this->size = size;
    this->capacity = capacity;
}

/* 定时器回调函数 */
void cb_func(client_data* user_data) {
    /* 删除非活动链接在socket上的注册事件 */
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    // 关闭文件描述符
    close(user_data->sockfd);

    // 减少连接数
    --http_conn::m_user_count;
}

void timer_heap::pop_timer() {
    array[1]->cb_func(array[1]->user_data);   // 执行回调函数 
    array[1] = array[size];   // 最后一个拿到头上, 然后向下调整堆
    array[1]->user_data->No_ = 1;
    siftdown(1);
}

void timer_heap::siftup(int k) {
    heap_timer *t = array[k];
    for(int i = k/2; i > 0; i /= 2) {
        if(t->expire >= array[i]->expire)
            break;
        array[k] = array[i];
        array[k]->user_data->No_ = k;
        k = i;
    }
    array[k] = t;
    array[k]->user_data->No_ = k;
}

void timer_heap::siftdown(int k) {
    heap_timer *t = array[k];
    for(int i = 2*k; i <= size; i *= 2) {
        if(i < size && array[i+1]->expire < array[i]->expire)
            i = i + 1;  // 与更小的交换
        if(t->expire < array[i]->expire) 
            break;
        array[k] = array[i];
        array[k]->user_data->No_ = k;
        k = i; 
    }
    array[k] = t;
    array[k]->user_data->No_ = k;
}
 
void timer_heap::add_timer(heap_timer* timer) {
    if(!timer) {
        return;
    }
    ++size;
    if(size > capacity) {
        capacity *= 2;
        array.resize(capacity);
    }
    timer->user_data->No_ = size;
    array[size] = timer;
    siftup(size);
}

void timer_heap::adjust_timer(int timer) {
    if(timer <= 0) return;

    array[timer]->expire += TIMELAG;
    siftdown(timer);
    // TODO
    // 重新定时
}

void timer_heap::del_timer(int timer) {
    /* 执行回调函数 */
    array[timer]->cb_func(array[timer]->user_data);
    array[timer] = array[size];
    array[timer]->user_data->No_ = timer;
    --size;
    siftdown(timer);
    siftup(timer);
}

/* 设置文件描述符非阻塞 */
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 向内核事件表注册读事件, ET模式下选择开启EPOLLONESHOT */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT; // EPOLLONESHOT: 一个文件描述符同一时刻只能被一个线程处理
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}



const heap_timer* timer_heap::top() {
    return array[0];
}

/* 信号处理函数 */
void Utils::sig_handler(int sig) {
    /*
        信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响
        
        为保证函数的可重入性，保留原来的errno
        可重入性：中断后再次进入改函数，环境变量与之前相同，不会丢失数据
     */
    int save_errno = errno;
    int msg = sig;

    /* 通过管道传递信号 */
    send(u_pipefd[1], (char *)&msg, 1, 0);

    /* 恢复原来的 errno */
    errno = save_errno;
}

/* 设置信号函数，仅关注SIGTERM和SIGALRM两个信号 */
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    /* 注意: 信号处理函数中仅仅发送信号值，不做对应逻辑处理 */
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    
    /* 将所有信号添加到信号集中 */
    sigfillset(&sa.sa_mask); // sa_mask用来指定在信号处理函数执行期间需要被屏蔽的信号

    /* 执行sigaction函数 */
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_heap::tick()  {
    time_t cur = time(NULL);
    while(size) {
        if(array[1]->expire > cur) break;
        array[1]->cb_func(array[1]->user_data);
        pop_timer();
        --size;
    }
}
/* 定时处理任务 */
void Utils::timer_handler() {
    heap.tick();
    int nextClock = heap.top()->expire - time(NULL);
    alarm(nextClock);
}

void Utils::show_error(int connfd, const char* info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

