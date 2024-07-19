#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
#include <cassert>
#include <chrono>
#include <cstddef>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <mutex>

template <typename T>
class BlockQueue{
public:
    explicit BlockQueue(std::size_t max_size = 1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void clear();
    void push_back(const T& elem);
    void push_front(const T& elem);
    bool pop_front(T& elem);
    bool pop_front(T& elem, int timeout);
    T front();
    T back();
    size_t capacity();
    size_t size();
    void flush();
    void Close();

private:

private:
    std::deque<T> deq_;                     // 底层数据结构
    size_t capacity_;                       // 容量
    std::mutex mtx_;                           // 互斥量
    std::atomic_bool is_close_;                 // 停止标志
    std::condition_variable cv_con_;        // 消费者条件变量
    std::condition_variable cv_pro_;        // 生产者条件变量
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t max_size) : 
capacity_(max_size),                            
is_close_(false)
{
    assert(capacity_ > 0);
}

template <typename T>
BlockQueue<T>::~BlockQueue(){
    Close();
}

template <typename T>
void BlockQueue<T>::Close(){
    clear();                                // 清空队列
    is_close_.store(true);                  // 置位停止标志
    cv_con_.notify_all();                   // 唤醒所有消费者线程
    cv_pro_.notify_all();                   // 唤醒所有生产者线程
}

template <typename T> 
void BlockQueue<T>::clear(){                // 清空队列
    std::lock_guard<std::mutex> lck(mtx_);
    deq_.clear();
}

template <typename T>
bool BlockQueue<T>::empty(){                // 队列为空？
    return deq_.empty();
}


// 队列为满？
template <typename T>
bool BlockQueue<T>::full(){
    std::lock_guard<std::mutex> lck(mtx_);
    return deq_.size() >= capacity_;
}


// 插入元素
template <typename T>
void BlockQueue<T>::push_back(const T& elem){
    std::unique_lock<std::mutex> lck(mtx_);
    cv_pro_.wait(lck,[this](){
        return is_close_.load() || deq_.size() < capacity_;         // 如果是关闭队列或者队列未满，则不阻塞
    });

    if(is_close_.load())                                    // 如果判断是is_close是true，则说明要关闭队列了
        return;

    deq_.push_back(elem);
    cv_con_.notify_one();
}

template <typename T>
void BlockQueue<T>::push_front(const T& elem){
    std::unique_lock<std::mutex> lck(mtx_);
    cv_pro_.wait(lck,[this](){
        return is_close_.load() || deq_.size() < capacity_;         // 同上一样的逻辑
    });

    if(is_close_.load())
        return;

    deq_.push_front(elem);
    cv_con_.notify_one();
}

// 弹出元素
template <typename T>
bool BlockQueue<T>::pop_front(T& elem){
    std::unique_lock<std::mutex> lck(mtx_);
    
    while(deq_.empty()){
        if(is_close_.load()){
            return false;
        }

        cv_con_.wait(lck);
    }
    cv_con_.wait(lck,[this](){
        return is_close_.load() || !deq_.empty();           // 如果停止或者队列不为空
    });

    if(is_close_.load())                // 如果停止标志被置位
        return false;

    elem = deq_.front();
    deq_.pop_front();
    cv_pro_.notify_one();
    return true;
}

template <typename T>
bool BlockQueue<T>::pop_front(T& elem, int timeout){
    std::unique_lock<std::mutex> lck(mtx_);
    while(deq_.empty()){
        if(cv_con_.wait_for(lck, std::chrono::seconds(timeout)) == std::cv_status::timeout)
            return false;

        if(is_close_.load())
            return false;
    }

    elem = deq_.front();
    deq_.pop_front();
    cv_pro_.notify_one();
    return true;
}

template <typename T>
T BlockQueue<T>::front(){
    std::lock_guard<std::mutex> lck(mtx_);
    return deq_.front();
}

template <typename T>
T BlockQueue<T>::back(){
    std::lock_guard<std::mutex> lck(mtx_);
    return deq_.back();
}

template <typename T>
size_t BlockQueue<T>::capacity(){
    std::lock_guard<std::mutex> lck(mtx_);
    return capacity_;
}

template <typename T>
size_t BlockQueue<T>::size(){
    std::lock_guard<std::mutex> lck(mtx_);
    return deq_.size();
}

template <typename T>
void BlockQueue<T>::flush(){
    cv_con_.notify_all();
}


#endif 