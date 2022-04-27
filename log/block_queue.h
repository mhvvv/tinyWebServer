#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "locker.h"

/*************************************************************
 * 循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;  
 * 线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
 **************************************************************/
template <typename T>
class block_queue {
public:
    block_queue(int max_size = 1000);
    ~block_queue();

    void clear();
    /* 判断队列是否满了 */
    bool full(); 
    /* 判断队列是否为空 */
    bool empty();
    /* 返回队首元素 */
    bool front(T &value);
    /* 返回队尾元素 */
    bool back(T &value);
    int size();
    int max_size();
    /* 
        往队列添加元素 
        以广播的方式唤醒所有等待目标条件变量的线程,
        若当前没有线程等待条件变量,则唤醒无意义
     */
    bool push(const T& iter);
    bool pop(T& item);
    void flush();

private:
    locker m_mutex;
    cond m_cond;

    T* m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

template <typename T>
block_queue<T>::block_queue(int max_size) {
    if(max_size <= 0) exit(1);

    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template <typename T>
block_queue<T>::~block_queue() {
    m_mutex.lock();
    if(m_array) delete []m_array;
    m_mutex.unlock();

    /* 其他成员会自动销毁 */
}

template <typename T>
void block_queue<T>::clear() {
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template <typename T>
bool block_queue<T>::full() {
    m_mutex.lock();
    if(m_size >= m_max_size) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T>
bool block_queue<T>::empty() {
    m_mutex.lock();
    if(m_size == 0) {
        m_mutex.unlock();
        return true;
    }    
    m_mutex.unlock();
    return false;
}

template <typename T>
bool block_queue<T>::front(T& value) {
    if(empty()) return false;
    m_mutex.lock();
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::back(T &value) {
    if(empty()) return false;
    m_mutex.lock();
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <typename T>
int block_queue<T>::size() {
    int _size;
    m_mutex.lock();
    _size = m_size;
    m_mutex.unlock();
    return _size;
}

template <typename T>
int block_queue<T>::max_size() {
    int _size;
    m_mutex.lock();
    _size = m_max_size;
    m_mutex.unlock();
    return _size; 
}

template <typename T>
bool block_queue<T>::push(const T& item) {
    /*
        以广播的方式唤醒所有等待目标条件变量的线程,
        若当前没有线程等待条件变量,则唤醒无意义
     */
    m_mutex.lock();
    if(m_size >= m_max_size) {  // 已满未处理 
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    ++m_size;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}   

template <typename T>
bool block_queue<T>::pop(T& item) {
    m_mutex.lock();
    while(m_size <= 0) { // 如果当前队列没有元素,将会等待条件变量
        if(!m_cond.wait(m_mutex.get())) {
            m_mutex.unlock();
            return false;
        }
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    --m_size;
    m_mutex.unlock();
    return true;
}


template <typename T>
void block_queue<T>::flush() {
    while(m_size != 0) {
        m_cond.broadcast();
    }
}
#endif  // BLOCK_QUEUE_H