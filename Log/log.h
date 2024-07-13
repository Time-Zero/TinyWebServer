#ifndef LOG_H
#define LOG_H
#include <atomic>
#include <climits>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "../common/nocopy.h"
#include "blockqueue.h"
#include "../Buffer/buffer.h"
#include <sys/stat.h>
#include <sys/time.h>

constexpr int LOG_PATH_LEN = 256;
constexpr int LOG_NAME_LEN = 256;
constexpr int LOG_MAX_LINE = 50000;

// 文件智能指针删除器
struct OutFileDeleter{
    void operator()(std::fstream* ptr){
        if(ptr == nullptr)
            return;
        
        ptr->flush();
        ptr->close();
        delete ptr;
        ptr = nullptr;
    }
};

class Log : public NoCopy{
public:
    void init(int level, 
        const char* path = "./log", 
        const char* suffix = ".log",
        const int max_queue_capacity = 1024
        );
    
    ~Log();
    static Log& instance();
    static void flush_log_thread();
    void flush();
    void write(int level, const char* format, ...);
    int get_level();
    void set_level(const int level);
    bool is_open();


private:
    Log();
    void async_write();
    void append_log_level_title(int level);

private:
    const char* path_;
    const char* suffix_;
    int level_;
    int to_day_;
    int sub_log_count_;
    std::atomic_int line_count_;
    std::mutex mtx_;
    std::atomic_bool is_async_;
    std::atomic_bool is_open_;
    std::unique_ptr<BlockQueue<std::string>> deque_;
    std::unique_ptr<std::thread> write_thread_;
    std::unique_ptr<std::fstream, OutFileDeleter> out_file_;
    Buffer buffer_;
};

#define LOG_BASE(level, format, ...) \
    do{\
        if(Log::instance().is_open() && Log::instance().get_level() <= level) { \
            Log::instance().write(level, format, ##__VA_ARGS__);\
            Log::instance().flush();\
        }\
    }while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)}while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)}while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)}while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)}while(0);

#endif