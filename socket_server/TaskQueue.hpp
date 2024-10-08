#ifndef __TaskQueue__
#define __TaskQueue__

#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <functional>

template <typename T>
class TaskQueue
{
private:
    std::queue<T> m_queue; // 利用模板函数构造队列
    std::mutex m_mutex;    // 访问互斥信号量

public:
    TaskQueue(){};
    ~TaskQueue(){};

    bool empty() // 返回队列是否为空
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 互斥信号变量加锁，防止m_queue被改变
        return m_queue.empty();
    }

    int size()
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 互斥信号变量加锁，防止m_queue被改变
        return m_queue.size();
    }

    // 队列添加元素 传入左值
    void enqueue(T &t)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace(t);
    }

    // 队列添加元素 传入右值
    void enqueue(T &&t)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.emplace(std::move(t));
    }

    // 队列取出元素
    bool dequeue(T &t)
    {
        std::lock_guard<std::mutex> lock(m_mutex);  // 队列加锁
        if (m_queue.empty())
        {
            return false;
        }
        t = std::move(m_queue.front()); // 取出队首元素，返回队首元素值，并进行右值引用
        m_queue.pop();                  // 弹出入队的第一个元素
        return true;
    }
};

#endif