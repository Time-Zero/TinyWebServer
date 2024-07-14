#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "nocopy.h"
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
#include <functional>

class ThreadPool : public NoCopy{
public:
    using Task = std::packaged_task<void()>;
    ~ThreadPool(){
        stop();
    }

    static ThreadPool& Instance(){
        static ThreadPool ins;
        return ins;
    }

    int IdelThreadNum(){
        return thread_num_;
    }

    template<class F, class ...Args>
    auto commit(F&& f, Args&& ...args) -> std::future<decltype(f(args...))> {
        using RetType = decltype(f(args...));
        if(stop_.load())
            return std::future<RetType>{};
        
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<RetType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> lck(mtx_);
            tasks_.emplace([task]{(*task)();});
        }
    
        cv_lock_.notify_one();
        return ret;
    }

private:
    ThreadPool(unsigned int num = std::thread::hardware_concurrency()){
        if(num == 1)
            thread_num_ = 2;
        else 
            thread_num_ = num;

        start();
    }
    
    void stop(){
        stop_.store(true);
        cv_lock_.notify_all();
        for(auto& td : pool_){
            if(td.joinable()){
                td.join();
            }
        }
    }

    void start(){
        for(int i = 0; i < thread_num_; ++i){
            pool_.emplace_back([this](){
                while(!this->stop_.load()){
                    Task task;
                    {
                        std::unique_lock<std::mutex> lck(mtx_);
                        this->cv_lock_.wait(lck, [this](){
                            return this->stop_.load() || !this->tasks_.empty();
                        });

                        if(this->tasks_.empty())
                            return;

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    thread_num_--;
                    task();
                    thread_num_++;
                }
            });
        }
    }

private:
    std::mutex mtx_;
    std::atomic_int thread_num_;
    std::atomic_bool stop_;
    std::condition_variable cv_lock_;
    std::queue<Task> tasks_;
    std::vector<std::thread> pool_;
};

#endif