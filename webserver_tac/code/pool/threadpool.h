#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <assert.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool
{
  public:
    explicit ThreadPool(size_t threadCount = 8) : pool_(std::make_shared<Pool>())
    {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; i++)
        {                                                       // 创建threadCount个线程
            std::thread([pool = pool_] {                        // 使用lambda表达式
                std::unique_lock<std::mutex> locker(pool->mtx); // 加锁，防止多个线程取到同一个任务
                while (true)
                { // 采用死循环
                    if (!pool->tasks.empty())
                    {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock(); // 对任务队列操作结束释放锁
                        task();          // 执行函数
                        locker.lock();   // 函数执行完毕，重新加锁
                    }
                    else if (pool->isClosed)
                        break;
                    else
                        pool->cond.wait(locker); // 线程阻塞等待被唤醒
                }
            })
                .detach();
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool &&) = default;

    ~ThreadPool()
    {
        if (static_cast<bool>(pool_))
        {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template <class F> void AddTask(F &&task)
    {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx); // 加入队列时加锁
            pool_->tasks.emplace(std::forward<F>(task));    // 将任务添加到队列中
        }
        pool_->cond.notify_one(); // 唤醒一个线程
    }

  private:
    struct Pool
    {
        std::mutex mtx;                          // 创建锁
        std::condition_variable cond;            // 创建条件变量
        bool isClosed = false;                   // 判断是否关闭线程
        std::queue<std::function<void()>> tasks; // 请求任务队列
    };
    std::shared_ptr<Pool> pool_;
};

#endif // THREADPOOL_H