#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const int NUM = 2;
// 任务结构体
typedef struct Task
{
    void (*function)(void *arg);
    void *arg;
} Task;

// 线程池结构体
struct ThreadPool
{
    // 任务队列，环形队列。
    Task *taskQ;
    int queueCapacity; // 容量
    int queueSize;     // 当前任务个数
    int queueFront;    // 队头 -> 取数据
    int queueRear;     // 队尾 -> 放数据

    pthread_t managerID;       // 管理者线程ID
    pthread_t *threadIDs;      // 工作的线程ID
    int minNum;                // 最小线程数量，不需要修改
    int maxNum;                // 最大线程数量，不需要修改
    int busyNum;               // 忙的线程的个数，需要经常修改
    int liveNum;               // 存活的线程的个数，不需要经常修改
    int exitNum;               // 要销毁的线程个数，不需要经常修改
    pthread_mutex_t mutexPool; // 锁整个的线程池，操作任务队列的头尾，大小
    pthread_mutex_t mutexBusy; // 锁busyNum变量，因为busyNum需要经常修改
    pthread_cond_t notFull;    // 任务队列是不是满了
    pthread_cond_t notEmpty;   // 任务队列是不是空了

    int shutdown; // 是不是要销毁线程池, 销毁为1, 不销毁为0
};

ThreadPool *threadPoolCreate(int min, int max, int queueCapacity)
{
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    do
    {
        // 创建整个池
        if (pool == NULL)
        {
            printf("malloc ThreadPool failed....");
            break;
        }

        // 初始化线程池的参数
        pool->threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL)
        {
            printf("malloc pthreadIDs failed....");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->liveNum = min;
        pool->busyNum = 0;
        pool->exitNum = 0;
        pool->shutdown = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0)
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        // 创建任务队列及其属性
        pool->taskQ = (Task *)malloc(sizeof(Task) * queueCapacity);
        pool->queueCapacity = queueCapacity;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        // 创建线程
        pthread_create(&pool->managerID, NULL, manager, pool); // 管理者线程
        for (int i = 0; i < min; ++i)                          // N个工作者线程，按照最小数创建(实质上应该按照liveNum创建)
        {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        return pool;
    } while (0);

    // 创建资源后的某一个环节除了问题，直接return会导致资源没被释放
    // 使用do while结构break出来，对资源进行释放再return，类似于goto。
    // 释放资源
    if (pool && pool->threadIDs)
        free(pool->threadIDs);
    if (pool && pool->taskQ)
        free(pool->taskQ);
    if (pool)
        free(pool);
    return NULL;
}

int threadPoolDestroy(ThreadPool *pool)
{
    if (pool == NULL)
        return -1;
    // 关闭线程池
    pool->shutdown = 1;
    // 管理线程的while循环退出
    pthread_join(pool->managerID, NULL);
    // 唤醒全部工作者线程
    for (int i = 0; i < pool->liveNum; i++)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    // 这里先等一会直到全部都没了再去清理资源
    // 因为自杀函数要用到threadIDs
    sleep(2);
    // 回收堆资源
    // 释放堆内存
    if (pool->taskQ)
    {
        free(pool->taskQ);
        pool->taskQ = NULL;
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
        pool->threadIDs = NULL;
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}

void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    // 任务队列已经满了，那就得阻塞，直到notFull为真。
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // 阻塞生产者线程
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    // 唤醒消费者
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

void *worker(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;

    // 不断去读工作队列并执行
    while (1)
    {
        pthread_mutex_lock(&pool->mutexPool);

        // 工作队列为空，且不是关闭状态，则需要阻塞
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            // 会阻塞工作队列且自动解锁mutex。当唤醒后又自动加锁。
            // 为什么while 循环，因为有可能多个工作者被唤醒，某个抢到了，那么size还是为0，还是得阻塞。
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // 被唤醒了，得看看是不是要自杀
            if (pool->exitNum > 0)
            {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }

        // 如果线程池被关闭了
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        // 唤醒之后开始工作,首先找到最前面的任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;

        // 维护环形队列
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        // 唤醒生产者，然后解锁
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        // 之前从线程池中获取了任务，现在开始执行任务。
        // 忙任务 + 1
        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        // 开始执行任务
        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;
        // 忙任务-1
        printf("thread %ld end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void *manager(void *arg)
{

    ThreadPool *pool = (ThreadPool *)arg;
    // 根据当前情况去调整线程数量
    while (!pool->shutdown)
    {
        sleep(3);

        // 取出线程数量和任务数量进行比较
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize; // 任务个数
        int liveNum = pool->liveNum;     // 线程个数
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 添加线程
        // 当前任务个数 > 线程数量 且 线程数量比最大数量小
        if (queueSize > liveNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int cnt = 0;
            for (int i = 0; i < pool->maxNum && cnt < NUM && pool->liveNum < pool->maxNum; i++)
            {
                if (pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    pool->liveNum++;
                    cnt++;
                }
            }
            printf("add %d thread success\n",cnt);
            pthread_mutex_unlock(&pool->mutexPool);
        }

        // 销毁线程
        // 忙的线程*2 < 线程数量 且 线程数量比最小数量大
        if (busyNum * 2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUM;
            pthread_mutex_unlock(&pool->mutexPool);
            // 通知工作的线程自杀
            for (int i = 0; i < NUM; i++)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }

    return NULL;
}

void threadExit(ThreadPool *pool)
{
    // 找到该线程的tid，然后将tid赋值为0之后退出
    // 如果直接退出的话，tid并没有变为0，下次就不能利用这个坑位继续创建了
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i)
    {
        if (pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}