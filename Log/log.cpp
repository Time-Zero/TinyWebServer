#include "log.h"
#include "blockqueue.h"
#include <bits/types/struct_timeval.h>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>

Log& Log::instance(){
    static Log ins;
    return ins;
}

Log::Log() : 
    out_file_(nullptr, OutFileDeleter()),
    deque_(nullptr),            // 当设置为异步写，才构建阻塞队列
    write_thread_(nullptr),
    line_count_(0),
    to_day_(0),
    is_async_(false),
    is_open_(false),
    level_(0),
    path_(nullptr),
    suffix_(nullptr),
    sub_log_count_(0)
{
    
}

Log::~Log(){
    while(deque_.get() != nullptr && !deque_->empty()){
        deque_->flush();
    }

    if(write_thread_.get() != nullptr && write_thread_->joinable()){
        write_thread_->join();
    }
}

void Log::flush(){
    if(is_async_.load()){
        deque_->flush();
    }

    out_file_->flush();
}

void Log::async_write(){
    std::string str = "";
    std::unique_lock<std::mutex> lck(mtx_);
    lck.unlock();
    while(deque_->pop_front(str)){
        lck.lock();
        (*out_file_) << str;
        (*out_file_).flush();
        lck.unlock();
    }
}

void Log::flush_log_thread(){
    Log::instance().async_write();
}

void Log::init(int level,
                const char* path,
                const char* suffix,
                const int max_queue_capacity
){
    is_open_.store(true);
    level_ = level;
    path_ = path;
    suffix_ = suffix;

    if(max_queue_capacity){     // 启用异步日志
        is_async_.store(true);          
        if(deque_.get() == nullptr){                // 如果队列是空指针，则初始化队列
            deque_.reset(new BlockQueue<std::string>);
            write_thread_.reset(new std::thread(flush_log_thread));         // 创建异步线程
        }
    }else{
        is_async_.store(false);             // 同步日志
    }

    // line_count_.store(0);
    time_t timer = time(nullptr);
    struct tm* sys_time = localtime(&timer);
    to_day_ = sys_time->tm_mday;

    std::stringstream ss;
    ss << path_ << "/" << std::setw(4) << std::setfill('0') << (sys_time->tm_year + 1900) << "_"
    << std::setw(2) << std::setfill('0') << (sys_time->tm_mon + 1) << "_" 
    << std::setw(2) << std::setfill('0') << sys_time->tm_mday << suffix_; 
    std::string file_name = ss.str();

    {
        std::lock_guard<std::mutex> lck(mtx_);
        buffer_.RetrieveAll();
        if(out_file_.get() == nullptr){             // 如果日志文件没有创建
            out_file_.reset(new std::fstream);      // 创建文件对象
            out_file_->open(file_name.c_str(), std::ios::out | std::ios::app);   // 尝试打开文件
        }

        if(out_file_->fail()){          // 文件打开错误
            int err_num = errno;                // 获取错误
            if(err_num == ENOENT){                  // 是路径不存在的问题
                mkdir(path_, 0777);                 // 创建路径
                out_file_->open(file_name.c_str(), std::ios::out | std::ios::app);       // 再次尝试打开文件
            }

            assert(out_file_->is_open());           // 断言，检测文件是不是打开的
            
        }
    }
}

void Log::write(int level, const char* format, ...){
    struct timeval now = {0,0};
    gettimeofday(&now, nullptr);
    time_t t_sec = now.tv_sec;
    struct tm* sys_time = localtime(&t_sec);
    struct tm t = *sys_time;
    va_list va_List;

    if(to_day_ != t.tm_mday || line_count_ > LOG_MAX_LINE){            // 如果是日期不匹配或者是行数超过log最大行数了
        std::unique_lock<std::mutex> lck(mtx_);
        lck.unlock();

        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << (t.tm_year + 1900) << "_" 
        << std::setw(2) << std::setfill('0') << (t.tm_mon + 1) << "_"
        << std::setw(2) << std::setfill('0') << t.tm_mday ;
        std::string str_tail = ss.str();
        ss.str("");

        std::string new_file_name;
        if(to_day_ != t.tm_mday){
            ss << path_ << "/" << str_tail << suffix_;
            to_day_ = t.tm_mday;
            line_count_.store(0);
        }else{
            sub_log_count_++;
            ss << path_ << "/" << str_tail << "-" << sub_log_count_ << suffix_;
            line_count_.store(0);
        }
        new_file_name = ss.str();

        lck.lock();
        out_file_.reset(new std::fstream);
        out_file_->open(new_file_name, std::ios::app | std::ios::in | std::ios::out);
        assert(!out_file_->fail());
    }

    {
        std::unique_lock<std::mutex> lck(mtx_);
        line_count_++;
        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << (t.tm_yday + 1900) << "-"
        << std::setw(2) << std::setfill('0') << (t.tm_mon + 1) << "-"
        << std::setw(2) << std::setfill('0') << t.tm_mday << " "
        << std::setw(2) << std::setfill('0') << t.tm_hour << ":"
        << std::setw(2) << std::setfill('0') << t.tm_min << ":"
        << std::setw(2) << std::setfill('0') << t.tm_sec << "."
        << std::setw(6) << std::setfill('0') << now.tv_usec;
        std::string str_time = ss.str();
        ss.clear();
        buffer_.Append(str_time);
        append_log_level_title(level);

        va_start(va_List,format);
        int m = vsnprintf(buffer_.BeginWrite(), buffer_.WritableBytes(), format, va_List);
        va_end(va_List);
        buffer_.HasWritten(m);

        buffer_.Append("\n",1);

        if(is_async_.load() && !deque_->full()){
            deque_->push_back(buffer_.RetrieveAllToStr());
        }else{
            out_file_->write(buffer_.Peek(), buffer_.ReadableBytes());
        }
        buffer_.RetrieveAll();
    }
}

void Log::append_log_level_title(int level){
    std::string label;
    switch (level) {
    case 0:
        label = " [DEBUG]: ";
        break;
    case 1:
        label = " [INFO]: ";
        break;
    case 2:
        label = " [WARN]: ";
        break;
    case 3:
        label = " [ERROR]: ";
        break;
    default:
        label = " [INFO]: ";
        break;
    }
    buffer_.Append(label);
}

int Log::get_level(){
    std::lock_guard<std::mutex> lck(mtx_);
    return level_;
}

void Log::set_level(const int level){
    std::lock_guard<std::mutex> lck(mtx_);
    this->level_ = level;
}

bool Log::is_open(){
    return is_open_.load();
}