#include "Buffer/buffer.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>

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

    return 0;
}