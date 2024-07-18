#include "heaptimer.h"
#include <bits/c++config.h>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <utility>

HeapTimer::HeapTimer() {
    heap_.reserve(64);
}

HeapTimer::~HeapTimer(){
    Clear();
}

void HeapTimer::Clear(){
    ref_.clear();
    heap_.clear();
}

void HeapTimer::SwapNode(size_t i, size_t j){
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i],heap_[j]);
    ref_[heap_[i].id] = i;          // 对应id存放的下标要做对应的修改
    ref_[heap_[j].id] = j;
}

// 元素上浮，给与一个节点的索引，如果这个节点的父节点值比这个元素的值大，就把这个元素和父节点位置交换，也就是对给定节点上浮
void HeapTimer::SiftUp(size_t i){
    assert(i >= 0 && i < heap_.size());
    size_t parent = (i -1) / 2; //求出这个节点的父节点
    while(1){           // 不断上浮，直到这个元素的父节点小于等于这个元素
        if(heap_[parent] > heap_[i]){
            SwapNode(i, parent);
            i = parent;
            parent = (i - 1) /2;
        }else{
            break;;
        }
    }
}

// false: 不需要下滑； true：下滑成功
// n: 是节点变化的范围，在指定数量的节点内下滑
bool HeapTimer::SiftDown(size_t i, size_t n){
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size());      
    
    size_t index = i;
    size_t child = 2 * index + 1;
    while(child < n){           // 如果
        if(child + 1 < n && heap_[child+1] < heap_[child]){     // 如果右孩子更小
            child++;
        }

        if(heap_[child] < heap_[index]){        // 如果当前节点的值还是比子节点的大，说明还需要下潜
            SwapNode(child, index);
            index = child;
            child = 2 * child + 1;
        }else{      // 如果当前节点的值小于等于子节点的值，说明这个元素的位置是合适的
            break;
        }
    }

    return index > i;           // 节点值变大了，所以也就i是节点交换成功了
}

// id:我们需要调整的堆的节点
void HeapTimer::Adjust(int id, int new_expires){
    assert(!heap_.empty() && ref_.count(id));           // 如果堆不为空，并且存在我们需要调整的元素
    heap_[ref_[id]].expires = Clock::now() + MS(new_expires);       // 这个new_expires是可以为负数，所以表示一个更早的时间点的
    if(!SiftDown(ref_[id], heap_.size())){          // 所以我们先尝试是不是需要下沉，如果不要下沉就上浮。实际在这里做这个判断没啥用，因为网络服务器肯定是加超时时间的，不太可能出现减超时时间
        SiftUp(ref_[id]);
    }
}

// 堆的删除就是把要删除的节点元素和堆最后一个元素交换，然后动态调整堆
void HeapTimer::Del(size_t index){
    assert(!heap_.empty() && index >= 0 && index < heap_.size());

    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);     //  要删除的元组肯定在堆的元素范围之内

    if(i < n){
        SwapNode(i, n);         // 把要删除元素和队尾元素交换
        if(!SiftDown(i, n)){            // 先对被交换过去的队尾元素进行下浮，如果不需要下浮，就说明两个子节点都比父节点小
            SiftUp(i);          // 那么我们再试试要不要上浮
        }

        ref_.erase(heap_.back().id);  
        heap_.pop_back();
    }
}

void HeapTimer::Add(int id, int time_out, const TimeoutCallBack& call_back_func){
    assert(id >= 0);

    if(ref_.count(id) == 1){        // 如果这个元素存在了，就调整
        int temp = ref_[id];
        heap_[temp].expires = Clock::now() + MS(time_out);
        heap_[temp].call_back_function = call_back_func;
        if(!SiftDown(temp,  heap_.size())){
            SiftUp(temp);
        }
    }else{
        size_t n = heap_.size();
        ref_[id] = n;       // 把id值赋值为当前堆的大小，也就是id最小
        heap_.push_back({id, Clock::now() + MS(time_out), call_back_func}); //已经是节点尾了，所以我们需要看看需不需要上浮
        SiftUp(n);

    }
}

// 执行回调函数，并且删除指定节点
void HeapTimer::DoWork(int id){
    if(heap_.empty() || ref_.count(id) == 0)
        return;

    size_t i = ref_[id];
    auto node = heap_[i];
    node.call_back_function();
    Del(i);
}

// 处理超时事件
void HeapTimer::Tick(){
    if(heap_.empty())
        return;

    while(!heap_.empty()){
        auto node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0){        // 把过期时间减去当前时间，看看是不是小于0
            break;      // 如果不是，就说明堆中的应该超时处理的节点已经被处理掉了
        }

        node.call_back_function();
        Pop();
    }
}

// 把堆顶元素删除
void HeapTimer::Pop(){
    assert(!heap_.empty());

    Del(0);
}

// 获取下一个超时事件会发生在多少ms之后，同时处理掉超时的事件
int HeapTimer::GetNextTick(){
    Tick();

    size_t res = -1;
    if(!heap_.empty()){
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();     // 超时时间点 - 当前时间
        if(res < 0 ) res = 0;
    }

    return res;
}