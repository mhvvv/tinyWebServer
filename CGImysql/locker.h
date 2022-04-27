#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* 封装信号量 */
class sem {
public:
    sem(int num = 0) {
        if(sem_init(&m_sem, 0, num) != 0)
            throw std::exception();
    }
    ~sem() {
        sem_destroy(&m_sem);
    }
    
    bool wait() {  // 信号量 P
        return sem_wait(&m_sem) == 0;
    }
    bool post() {  // 信号量 V
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem; // 信号量
};

/* 封装互斥锁 */
class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) 
            throw std::exception();
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() {  // 请求进入临界区
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock() {   // 离开临界区
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get() {  // 获取锁
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;  // 互斥量
};

/* 封装条件变量 */
class cond {
public:
    cond() {
        if(pthread_cond_init(&m_cond, NULL) != 0) 
            throw std::exception();
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex) {  // 等待条件变量, 内含一个mutex
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }
    bool signal() {  // 唤醒等待条件变量的线程 
        return pthread_cond_signal(&m_cond) == 0;
    }   
    bool broadcast() {   // 唤醒等待条件变量的线程 
        return pthread_cond_broadcast(&m_cond) == 0;
    }  

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif // LOCKER_H