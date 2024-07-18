#include "Buffer/buffer.h"
#include "Log/blockqueue.h"
#include "Log/log.h"
#include "Pool/threadpool.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <cstdio>
#include <unistd.h>
#include "Combine/webserver.h"

int main(){
    #if _BUFFER_TEST
    std::cout << "----------------Buffer Test--------------------"<<std::endl;
    Buffer buffer;
    std::cout << "Writable Bytes: " << buffer.WritableBytes() << std::endl;
    std::cout << "Readable Bytes: " << buffer.ReadableBytes() << std::endl;

    FILE* in_file = fopen("./test/test.txt", "r");
    if(!in_file){
        return 1;
    }

    int in_fd = fileno(in_file);
    int *err = nullptr;
    buffer.ReadFd(in_fd, err);
    if(err){
        std::cerr << "ReadFd Error" << std::endl;
    }
    std::cout << "Writable Bytes: " << buffer.WritableBytes() << std::endl;
    std::cout << "Readable Bytes: " << buffer.ReadableBytes() << std::endl;

    FILE *out_file = fopen("./test/test_out.txt", "a");
    int out_fd = fileno(out_file);
    err = nullptr;
    buffer.WriteFd(out_fd, err);
    if(err){
        std::cerr << "WriteFd Error" << std::endl;
    }
    std::cout << "Writable Bytes: " << buffer.WritableBytes() << std::endl;
    std::cout << "Readable Bytes: " << buffer.ReadableBytes() << std::endl;


    fclose(in_file);
    close(in_fd);
    fclose(out_file);
    close(out_fd);
    std::cout << "----------------End Buffer Test--------------------"<<std::endl;
    #endif

    #if _BLOCKQUEUE_TEST
    BlockQueue<int> block_queue;
    for(int i = 0; i < 100 ; i++){
        std::thread([&](){
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            block_queue.push_back(i);
            // std::cout << "The element insert is " << i << std::endl;
        }).detach();
    }

    if(block_queue.empty()){
        std::cout << "blockqueue is empty" << std::endl;
    }

    for(int i = 0; i < 100; i++){
        std::thread([&](){
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            int j = 0;
            block_queue.pop_front(j);
            // std::cout << "The element value is " << j << std::endl;
        }).detach();
    }
    #endif

    #if _LOG_TEST
        Log::instance().init(0,"./testlog", ".log",0);
        for(int i = 0; i < 1000; i++){
            // LOG_BASE(i % 4, "%s============%d", "hello", i);
            LOG_ERROR("Hello world");
        }
    #endif

    #if _THREADPOOL_TEST
    Log::instance().init(0,"./testlog", ".log",0);
    for(int i = 0 ; i < 4; i++){
        ThreadPool::Instance().commit([](){
            for(int i = 0; i < 10000; i++){
                LOG_BASE(i % 4, "%s============%d", "hello", i);
            }
        });
    }

    #endif

    WebServer server{1316,3,60000, 
                true, 3306, 
                "root","334859","webserver",12,true, 0, 1024};
    server.Start();

    return 0;
}