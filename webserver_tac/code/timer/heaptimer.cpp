/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */
#include "heaptimer.h"

// 结点交换，除了在数组中交换位置之外
// 还要更新哈希表值,以便根据id快速找到这个定时器在vector中的位置。
void HeapTimer::SwapNode_(size_t i, size_t j)
{
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

// 上滤，让原先在i位置的结点去到它应该去的地方。
// 就是一直与其父节点(i-1)/2比较，如果i比较小，则交换。
void HeapTimer::siftup_(size_t i)
{
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while (j >= 0 && i > 0 && heap_[j] < heap_[i])
    {
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 下滤，就是让index下沉到一个合适的位置
// 就是比较index的左右节点，选择一个较小的位置进行交换
bool HeapTimer::siftdown_(size_t index, size_t n)
{
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;     // 父节点位置
    size_t j = i * 2 + 1; // 左节点位置
    while (j < n)
    {
        if (j + 1 < n && heap_[j + 1] < heap_[j])
            j++; // 选择右节点，因为右节点较小
        if (heap_[i] < heap_[j])
            break;
        SwapNode_(i, j);
        i = j;         // 现在index到了j的位置，需要让i继续表示父节点，
        j = i * 2 + 1; // 根据新的父节点，计算出左结点
    }
    // 最后i位置的结点其实就是原先index位置的结点。
    // 如果原先index的值就比左右结点小，就不会移动，也就是如果没有下移，i==index，返回flase
    // 如果下移了，就一定有i>index，返回true
    return i > index;
}

/* 删除指定位置的结点 */
void HeapTimer::del_(size_t index)
{
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n)
    {
        SwapNode_(i, n);
        if (!siftdown_(i, n))
        {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack &cb)
{
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0)
    {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    }
    else
    {
        /* 已有结点：调整堆 */
        adjust(id, timeout);
        heap_[ref_[id]].cb = cb;
    }
}

void HeapTimer::doWork(int id)
{
    /* 删除指定id结点，并触发回调函数 */
    if (heap_.empty() || ref_.count(id) == 0)
    {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::adjust(int id, int timeout)
{
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick()
{
    /* 清除超时结点 */
    if (heap_.empty())
    {
        return;
    }
    while (!heap_.empty())
    {
        TimerNode node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop()
{
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear()
{
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick()
{
    tick();
    LOG_INFO("One Tick Check...");
    size_t res = -1;
    if (!heap_.empty())
    {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0)
        {
            res = 0;
        }
    }
    return res;
}