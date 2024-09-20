// C语言线程池
// 参考 爱编程的大丙
// 写于2024.02.24 - 2024.02.25

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

void taskFun(void *arg)
{
    int num = *(int *)arg;
    printf("thread %ld is working, number = %d\n", pthread_self(), num);
    sleep(1);
}

int main()
{
    ThreadPool *pool = threadPoolCreate(3, 10, 100);
    for (int i = 0; i < 100; i++)
    {
        int *p = (int*)malloc(sizeof(int));
        *p = i + 100;
        threadPoolAdd(pool, taskFun, p);
    }
    sleep(50);
    threadPoolDestroy(pool);
}
