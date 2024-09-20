#ifndef __ThreadPool__
#define __ThreadPool__

#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <chrono>

#include "TaskQueue.hpp"
using namespace std;

class ThreadPool
{
    using task_t = function<void()>;

private:
    vector<thread> m_threads;      // 工作线程队列
    TaskQueue<task_t> m_queue;     // 任务队列
    condition_variable m_notEmpty; // 条件变量
    mutex m_mutex;                 // 互斥锁
    mutex m_printmutex;            // 打印用的互斥锁，防止抢占输出
    bool m_shutdown;               // 是否关闭

    // 线程对应的工作函数----取任务，执行任务
    static void worker(ThreadPool *pool, const int id)
    {
        task_t task;   // 定义任务 task
        bool dequeued; // 是否成功取出队列中元素
        this_thread::sleep_for(chrono::seconds(1));
        while (!pool->m_shutdown)
        {
            {
                // 为线程环境加锁，互访问工作线程的休眠和唤醒
                unique_lock<mutex> lock(pool->m_mutex);

                // 如果任务队列为空，阻塞当前线程
                pool->m_notEmpty.wait(lock, [&]()
                                      {
                    unique_lock<mutex> lock(pool->m_printmutex);
                    if( pool->m_shutdown)
                        cout << "Thread " << id + 1 << " is Call to Destroy..." << endl;
                    else if(pool->m_queue.empty())
                        cout << "Thread " << id + 1 << " is waiting..." << endl;
                    return pool->m_shutdown || !pool->m_queue.empty(); });
            }

            // 取出任务队列中的元素
            dequeued = pool->m_queue.dequeue(task);
            // 如果成功取出，执行工作函数
            if (dequeued)
            {
                {
                    unique_lock<mutex> lock(pool->m_printmutex);
                    cout << "Thread " << id + 1 << " is working..." << endl;
                }
                task();
                {
                    unique_lock<mutex> lock(pool->m_printmutex);
                    cout << "Thread " << id + 1 << " is finished..." << endl;
                }
            }
        }
    }

public:
    // 线程池构造函数
    ThreadPool(int n_threads = 4) : m_shutdown(false)
    {
        for (int i = 0; i < n_threads; ++i)
        {
            m_threads.emplace_back(thread(worker, this, i)); // 创建工作线程
            unique_lock<mutex> lock(m_printmutex);
            cout << "Creating " << i + 1 << "th Threads" << endl;
        }
    }

    // 线程池析构函数
    ~ThreadPool()
    {
        m_shutdown = true;
        // 唤醒所有工作线程销毁自己
        {
            unique_lock<mutex> lock(m_printmutex);
            cout << "Waking Thread to Release the pool..." << endl;
        }
        m_notEmpty.notify_all();

        // 等待所有线程结束工作
        for (int i = 0; i < m_threads.size(); ++i)
        {
            if (m_threads.at(i).joinable())
            {
                m_threads.at(i).join();
            }
        }
        {
            unique_lock<mutex> lock(m_printmutex);
            cout << "The Pool has been Released!" << endl;
        }
    }

    // 线程池提交任务函数
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> future<decltype(f(args...))>
    {
        using return_t = decltype(f(args...));

// 复杂版本
        // 1.可以选择使用bind
        // function<return_t()> task = bind(forward<F>(f), forward<Args>(args)...);// 连接函数和参数定义，特殊函数类型，避免左右值错误
        // 2.也可以选择使用lambda将函数封装为 return_t (void) 类型。
        // function<return_t()> task = [=]()
        // {
        //     return f(args...);
        // };

        // auto task_ptr = make_shared<packaged_task<return_t()>>(task);

        // task_t warpper_task = [task_ptr]()
        // {
        //     (*task_ptr)();
        // };

        // m_queue.enqueue(warpper_task);

// 简化版本
        // auto task_ptr = make_shared<packaged_task<return_t()>>(bind(forward<F>(f), forward<Args>(args)...));
        auto task_ptr = make_shared<packaged_task<return_t()>>([f,args...](){ return f(args...);});
        m_queue.enqueue([task_ptr](){(*task_ptr)();});
// 结束

        m_notEmpty.notify_one();
        return task_ptr->get_future();
    }
};

#endif