#ifndef __TASKQUEUE__
#define __TASKQUEUE__

#include <pthread.h>
#include <queue>

using callback = void (*)(void *);
template <typename T>
struct Task
{
    callback function;
    T *arg;

    Task() : function(nullptr), arg(nullptr) {}
    Task(callback func, void *arg) : function(func), arg((T *)arg) {}
};

template <typename T>
class TaskQueue
{
private:
    std::queue<Task<T>> m_queue;
    pthread_mutex_t m_mutex; // 互斥锁

public:
    TaskQueue(/* args */)
    {
        pthread_mutex_init(&m_mutex, NULL);
    };
    ~TaskQueue()
    {
        pthread_mutex_destroy(&m_mutex);
    };

    void addTask(Task<T> &task)
    {
        pthread_mutex_lock(&m_mutex);
        m_queue.push(task);
        pthread_mutex_unlock(&m_mutex);
    };

    void addTask(callback func, void *arg)
    {
        pthread_mutex_lock(&m_mutex);
        Task<T> task;
        task.function = func;
        task.arg = arg;
        m_queue.push(task);
        pthread_mutex_unlock(&m_mutex);
    };

    Task<T> takeTask()
    {
        Task<T> task;
        pthread_mutex_lock(&m_mutex);
        if (!m_queue.empty())
        {
            task = m_queue.front();
            m_queue.pop();
        }
        pthread_mutex_unlock(&m_mutex);
        return task;
    };

    inline size_t taskNumber()
    {
        return m_queue.size();
    }
};

#endif
