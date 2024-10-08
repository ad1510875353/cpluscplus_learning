#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>
#include "ThreadPool.h"
using namespace std;

template <typename T>
ThreadPool<T>::ThreadPool(int minNum, int maxNum)
{
    do
    {
        // 创建任务队列
        m_taskQ = new TaskQueue<T>;

        // 初始化线程池
        m_minNum = minNum;
        m_maxNum = maxNum;
        m_busyNum = 0;
        m_aliveNum = minNum;
        m_shutdown = false;

        // 线程池ID列表
        m_threadIDs = new pthread_t[maxNum];
        if (m_threadIDs == nullptr)
        {
            cout << "new thread_t[] 失败...." << endl;
            break;
        }
        // 线程ID列表初始化
        memset(m_threadIDs, 0, sizeof(pthread_t) * maxNum);
        // 初始化互斥锁,条件变量
        if (pthread_mutex_init(&m_lock, NULL) != 0 ||
            pthread_cond_init(&m_notEmpty, NULL) != 0)
        {
            cout << "init mutex or condition fail..." << endl;
            break;
        }

        // 根据最小线程个数, 创建线程
        for (int i = 0; i < minNum; ++i)
        {
            pthread_create(&m_threadIDs[i], NULL, worker, this);
            cout << "创建子线程, ID: " << to_string(m_threadIDs[i]) << endl;
        }
        // 创建管理者线程, 1个
        pthread_create(&m_managerID, NULL, manager, this);
        return;
    } while (0);

    if (m_taskQ)
        delete m_taskQ;
    if (m_threadIDs)
        delete[] m_threadIDs;
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    m_shutdown = true;
    // 销毁管理者线程
    pthread_join(m_managerID, NULL);
    // 唤醒所有消费者线程
    // for (int i = 0; i < m_aliveNum; ++i)
    // {
    //     pthread_cond_signal(&m_notEmpty);
    // } // 逐个唤醒
    pthread_cond_broadcast(&m_notEmpty); // 全部唤醒
    for (int i = 0; i < m_maxNum; ++i)
    {
        if (m_threadIDs[i] != 0)
            pthread_join(m_threadIDs[i], NULL);
    }
    if (m_taskQ)
        delete m_taskQ;
    if (m_threadIDs)
        delete[] m_threadIDs;
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_notEmpty);
}

template <typename T>
void ThreadPool<T>::addTask(Task<T> task)
{
    if (m_shutdown)
    {
        return;
    }
    // 添加任务，不需要加锁，任务队列中有锁
    m_taskQ->addTask(task);
    // 唤醒工作的线程
    pthread_cond_signal(&m_notEmpty);
}

template <typename T>
void ThreadPool<T>::addTask(callback fun, void *arg)
{
    if (m_shutdown)
    {
        return;
    }
    // 添加任务，不需要加锁，任务队列中有锁
    Task<T> task(fun, arg);
    m_taskQ->addTask(task);
    // 唤醒工作的线程
    pthread_cond_signal(&m_notEmpty);
}

template <typename T>
int ThreadPool<T>::getAliveNumber()
{
    int threadNum = 0;
    pthread_mutex_lock(&m_lock);
    threadNum = m_aliveNum;
    pthread_mutex_unlock(&m_lock);
    return threadNum;
}

template <typename T>
int ThreadPool<T>::getBusyNumber()
{
    int busyNum = 0;
    pthread_mutex_lock(&m_lock);
    busyNum = m_busyNum;
    pthread_mutex_unlock(&m_lock);
    return busyNum;
}

template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool<T> *pool = static_cast<ThreadPool<T> *>(arg);
    // 一直不停的工作
    while (true)
    {
        // 访问任务队列(共享资源)加锁
        pthread_mutex_lock(&pool->m_lock);

        // 判断任务队列是否为空, 如果为空工作线程阻塞
        while (pool->m_taskQ->taskNumber() == 0 && !pool->m_shutdown)
        {
            cout << "thread " << to_string(pthread_self()) << " waiting..." << endl;
            // 阻塞线程
            pthread_cond_wait(&pool->m_notEmpty, &pool->m_lock);

            // 解除阻塞之后, 判断是否要销毁线程
            if (pool->m_exitNum > 0)
            {
                pool->m_exitNum--;
                if (pool->m_aliveNum > pool->m_minNum)
                {
                    pool->m_aliveNum--;
                    pthread_t tid = pthread_self();
                    for (int i = 0; i < pool->m_maxNum; ++i)
                    {
                        if (pool->m_threadIDs[i] == tid)
                        {
                            pool->m_threadIDs[i] = 0;
                            break;
                        }
                    }
                    cout << "threadExit() function: thread "
                         << to_string(pthread_self()) << " exiting..." << endl;
                    pthread_mutex_unlock(&pool->m_lock);
                    pool->threadExit();
                }
            }
        }
        // 判断线程池是否被关闭了
        if (pool->m_shutdown)
        {
            cout << "threadExit() function: thread "
                 << to_string(pthread_self()) << " exiting..." << endl;
            pthread_mutex_unlock(&pool->m_lock);
            pool->threadExit();
        }

        // 从任务队列中取出一个任务
        Task<T> task = pool->m_taskQ->takeTask();
        // 工作的线程+1
        pool->m_busyNum++;

        // 执行任务log，,放在锁里面避免打印混乱
        cout << "thread " << to_string(pthread_self()) << " start working..." << endl;
        // 线程池解锁，然后执行任务
        pthread_mutex_unlock(&pool->m_lock);
        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        pthread_mutex_lock(&pool->m_lock);
        // 任务处理结束log,放在锁里面避免打印混乱
        cout << "thread " << to_string(pthread_self()) << " end working..." << endl;
        pool->m_busyNum--;
        pthread_mutex_unlock(&pool->m_lock);
    }
    return nullptr;
}

template <typename T>
void *ThreadPool<T>::manager(void *arg)
{
    ThreadPool<T> *pool = static_cast<ThreadPool<T> *>(arg);
    // 如果线程池没有关闭, 就一直检测
    while (!pool->m_shutdown)
    {
        // 每隔3s检测一次
        sleep(3);
        // 取出线程池中的任务数和线程数量
        pthread_mutex_lock(&pool->m_lock);
        int queueSize = pool->m_taskQ->taskNumber();
        int liveNum = pool->m_aliveNum;
        int busyNum = pool->m_busyNum;
        pthread_mutex_unlock(&pool->m_lock);

        // 创建线程
        const int NUMBER = 2;
        // 当前任务个数>存活的线程数 && 存活的线程数<最大线程个数
        if (queueSize > liveNum && liveNum < pool->m_maxNum)
        {
            // 线程池加锁
            pthread_mutex_lock(&pool->m_lock);
            int cnt = 0;
            for (int i = 0; i < pool->m_maxNum && cnt < NUMBER && pool->m_aliveNum < pool->m_maxNum; ++i)
            {
                if (pool->m_threadIDs[i] == 0)
                {
                    pthread_create(&pool->m_threadIDs[i], NULL, worker, pool);
                    cnt++;
                    pool->m_aliveNum++;
                }
            }
            cout << "Creating " << cnt << " Thread Success ! Alive Thread = " << pool->m_aliveNum << endl;
            pthread_mutex_unlock(&pool->m_lock);
        }

        // 销毁多余的线程
        // 忙线程*2 < 存活的线程数目 && 存活的线程数 > 最小线程数量
        if (busyNum * 2 < liveNum && liveNum > pool->m_minNum)
        {
            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = NUMBER;
            pthread_mutex_unlock(&pool->m_lock);
            for (int i = 0; i < NUMBER; ++i)
            {
                pthread_cond_signal(&pool->m_notEmpty);
            }
        }
    }
    return nullptr;
}

// 这里对比大丙那作了删改，先放着吧
template <typename T>
void ThreadPool<T>::threadExit()
{
    pthread_exit(NULL);
}
