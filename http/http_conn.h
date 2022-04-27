#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "locker.h"
#include "sql_connection_pool.h"
#include "lst_timer.h"
#include "log.h"

class http_conn {
public:
    static const int FILENAME_LEN = 256;        // 要读取文件的路径 + 名称 m_read_file 长度
    static const int READ_BUFFER_SIZE = 256;    // 读缓冲区大小 m_read_buf 大小
    static const int WRITE_BUFFER_SIZE = 256;   // 写缓冲区 w_read_buf 大小
    enum METHOD {       // 报文请求方法集合, 只用到GET 和 POST
        GET = 0, POST, HEAD, PUT,
        DELETE, TRACE, OPTIONS, CONNECT, PATH
    };
    enum CHECK_STATE {  // 主状态机状态
        CHECK_STATE_REQUESTLINE = 0,  // 解析请求行
        CHECK_STATE_HEADER,           // 解析请求头
        CHECK_STATE_CONTENT           // 解析消息体. 我们认为只有POST请求报文含有消息体
    };
    enum LINE_STATUS {  // 从状态机状态
        LINE_OK = 0,  // 完整读取一行
        LINE_BAD,     // 报文语法有误
        LINE_OPEN     // 读取的行不完整
    };
    enum HTTP_CODE {  // 报文解析结果
        NO_REQUEST,           // 表示请求不完整，需要继续接收请求数据, 跳转主线程继续监测读事件
        GET_REQUEST,          // 得了完整的HTTP请求, 调用do_request完成请求资源映射
        BAD_REQUEST,          // HTTP请求报文有语法错误或请求资源为目录, 跳转process_write完成响应报文
        NO_RESOURCE,          // 请求资源不存在, 跳转process_write完成响应报文
        FORBIDDEN_REQUEST,    // 请求资源禁止访问，没有读取权限, 跳转process_write完成响应报文
        FILE_REQUEST,         // 请求资源可以正常访问, 跳转process_write完成响应报文
        INTERNAL_ERROR,       // 服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };

public:
    /* 初始化套接字地址, 函数内部会调用私有方法 init() */
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, std::string user, std::string passwd, std::string sqlname);
    /* 关闭 http 连接 */
    void close_conn(bool real_close = true);

    void process();
    
    /* 读取浏览器端发来的全部数据 */
    bool read_once(); 
    /* 响应报文写入函数 */
    bool write();
    
    sockaddr_in *get_address() {
        return &m_address;
    }
    /* 
        同步线程初始化数据库读取表 
        将数据库中已有的user信息读取到本地map中
    */
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;
    
private:
    void init();
    // 从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    // 向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);

    /* 解析http请求行, 获得请求方法、目标url及http版本号 */
    HTTP_CODE parse_request_line(char *text);
    // 主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    // 主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    
    // 生成响应报文
    HTTP_CODE do_request();

    /*
        * m_start_line 是已经解析的字符
        * get_line 用于将指针向后偏移，指向未处理的字符
        - 此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析
    */
    char *get_line() { 
        return m_read_buf + m_start_line; 
    };

    /* 
        从状态机，用于分析出一行内容
        返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
     */
    LINE_STATUS parse_line();
    
    void unmap();

    // 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;  
    static int m_user_count;  // 建立的TCP连接数量
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;  // epoll例程
    sockaddr_in m_address;

    // 存储读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];

    // 缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int m_read_idx;
    
    // m_read_buf读取的位置m_checked_idx
    int m_checked_idx;

    // m_read_buf中已经解析的字符个数
    int m_start_line;

    //存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    //指示buffer中的长度
    int m_write_idx;

    // 主状态机的状态
    CHECK_STATE m_check_state;  // 解析 头/行/主体
    // 请求方法 
    METHOD m_method;  // 请求行 - GET / POST
    /* 
        以下为解析请求报文中对应的6个变量
        存储读取文件的名称
    */
    char m_real_file[FILENAME_LEN];
    char *m_url;              // 请求行 - URL
    char *m_version;          // 请求行 - HTTP 1.1
    char *m_host;             // 服务器域名
    int m_content_length;     // 请求头部内容长度
    bool m_linger;            // 长连接 / 短链接

    char *m_file_address;     // 读取服务器上的文件地址
    struct stat m_file_stat;
    struct iovec m_iv[2];     // io向量机制iovec
    int m_iv_count;
    int cgi;                  // 是否启用的POST
    char *m_string;           // 存储请求头数据   账号 & 密码
    int bytes_to_send;        // 剩余发送字节数
    int bytes_have_send;      // 已发送字节数
    char *doc_root;

    std::map<std::string, std::string> m_users;
    int m_TRIGMode; // LT / ET
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};



#endif // HTTPCONNECTION_H