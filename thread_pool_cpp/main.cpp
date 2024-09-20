// 完成于2024.02.25
// C++版本线程池 同样参考爱编程的大丙
// 创建了工作队列类和线程池类，但是仍然使用posix接口
// 接下来打算做一个利用C++11版本库的线程池

#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>
#include "ThreadPool.cpp"

void taskFunc(void *arg)
{
    int num = *(int *)arg;
    printf("thread %ld is working, number = %d\n", pthread_self(), num);
    sleep(1);
}

int main()
{
    // 创建线程池
    ThreadPool<int> pool(3, 10);

    for (int i = 0; i < 100; ++i)
    {
        int *num = new int(100 + i);
        pool.addTask(taskFunc, num);
    }

    sleep(30);

    return 0;
}
