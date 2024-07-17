#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <chrono>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode{
    int id;
    TimeStamp expires;      // 超时时间点
    TimeoutCallBack call_back_function;     // 超时回调函数
    
    bool operator< (const TimerNode& t) {
        return expires < t.expires;
    }

    bool operator> (const TimerNode& t){
        return expires > t.expires;
    }
};

class HeapTimer{
public:
    HeapTimer();
    ~HeapTimer();

    void Clear();
    void Adjust(int id, int new_expires);
    void Add(int id, int time_out, const TimeoutCallBack& call_back_func);
    void DoWork(int id);
    void Tick();
    void Pop();
    int GetNextTick();

private:
    void SiftUp(size_t i);
    bool SiftDown(size_t i, size_t n);
    void SwapNode(size_t i, size_t j);
    void Del(size_t index);

private:
    std::vector<TimerNode> heap_;
    std::unordered_map<int, size_t> ref_;   // int: 就是堆结构体中的id，size_t就是对应结构体在heap_中的下标
};

#endif