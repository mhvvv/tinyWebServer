#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

/* 
    单例模式:
    
    单例模式作为最常用的设计模式之一，保证一个类仅有一个实例，并提供一个访问它的全局访问点，该实例被所有程序模块共享。

    实现思路：私有化它的构造函数，以防止外界创建单例类的对象；使用类的私有静态指针变量指向类的唯一实例，并用一个公有的静态方法获取该实例。

    单例模式有两种实现方法，分别是懒汉和饿汉模式。顾名思义，懒汉模式，即非常懒，不用的时候不去初始化，所以在第一次被使用时才进行初始化；饿汉模式，即迫不及待，在程序运行时立即初始化。
 */


class Log {  
public:
    /* C++ 11开始, 使用局部变量懒汉不用加锁 */
    static Log* get_instance();
    static void* flush_log_thread(void* args);

    /* 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列 */
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    
    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();

    void* async_write_log();

private:
    char dir_name[128];                      // 路径名
    char log_name[128];                      // log文件名
    int m_split_lines;                       // 日志最大行数
    int m_log_buf_size;                      // 日志缓冲区大小
    long long m_count;                       // 日志行数记录
    int m_today;                             // 因为按天分类,记录当前时间是那一天
    FILE *m_fp;                              // 打开log的文件指针
    char *m_buf;        
    block_queue<std::string> *m_log_queue;   // 阻塞队列
    bool m_is_async;                         // 是否同步标志位 true: 异步
    locker m_mutex;
    int m_close_log;                         // 关闭日志


};

Log* Log::get_instance() {
    static Log instance;
    return &instance;
}

void* Log::flush_log_thread(void* args) {
    Log::get_instance()->async_write_log();
} 

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif  // LOG_H