// 基于C++ 11实现简易线程池
// 使用可变参数模板，线程池提交任务方法与创建thread相同。
// 使用异步操作，使用packaged_task包装任务，提交任务时返回future以便获得任务返回值。
// 实现多线程安全的任务队列,使用lambda与function包装任务,使用RAII管理线程池的生命周期。
// 2024.02.29完成
// 缺点：没有实现管理者线程，增加线程容易，如何删除线程呢，使用哈希表存储id和线程。

#include <iostream>
#include <thread>

#include "TaskQueue.hpp"
#include "ThreadPool.hpp"

using namespace std;

mutex _m;

auto fun = [](int min, int max)
{
    int sum = 0;
    for (int i = min; i <= max; i++)
        sum += i;
    cout << sum << endl;
    return sum;
};

int main(int, char **)
{
    ThreadPool pool(2);
    int n = 20;
    vector<future<int>> ans;
    for (int i = 0; i < n; i++)
    {
        ans.emplace_back(pool.submit(fun, i * 5 + 1, (i + 1) * 5));
    }

    int sum = 0;
    for (int i = 0; i < n; i++)
    {
        ans[i].wait();
        sum += ans[i].get();
    }

    cout << "sum = " << sum << endl;

    sleep(1);

    return 0;
}
