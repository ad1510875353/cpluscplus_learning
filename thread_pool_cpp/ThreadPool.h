#ifndef __ThreadPool__
#define __ThreadPool__

#include "TaskQueue.hpp"

template <typename T> 
class ThreadPool
{
public:
    ThreadPool(int minNum, int maxNum);
    ~ThreadPool();

    // 添加任务
    void addTask(Task<T> task);
    void addTask(callback fun, void* arg);
    // 获取忙线程的个数
    int getBusyNumber();
    // 获取活着的线程个数
    int getAliveNumber();

private:
    // 工作的线程的任务函数
    static void* worker(void* arg);
    // 管理者线程的任务函数
    static void* manager(void* arg);
    void threadExit();

private:
    pthread_mutex_t m_lock;
    pthread_cond_t m_notEmpty;
    pthread_t* m_threadIDs;
    pthread_t m_managerID;
    TaskQueue<T>* m_taskQ;
    int m_minNum;
    int m_maxNum;
    int m_busyNum;
    int m_aliveNum;
    int m_exitNum;
    bool m_shutdown = false;
};


#endif