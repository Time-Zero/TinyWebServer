#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <arpa/inet.h>
#include <atomic>
#include <bits/types/struct_iovec.h>
#include <netinet/in.h>
#include <sys/types.h>
#include "../Buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn{
public:
    HttpConn();
    ~HttpConn();

    void Init(int fd, const sockaddr_in& addr);
    void Close(); 
    const char* GetIp() const;
    int GetPort() const;
    int GetFd() const;
    int ToWriteBytes();
    sockaddr_in GetAddr() const;
    bool process();
    ssize_t read(int* save_errno);
    ssize_t write(int* save_errno);
    
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }
    
    static void SetSrcDir(const char* src_dir);
    static void SetSrcDir(const std::string& src_dir);
    static void SetUserCount(const int user_count);
    static void SetIsEt(bool is_ET);
    static int GetUserCount() {return user_count_;}

private:

private:
    int fd_;
    struct sockaddr_in addr_;

    bool is_close_;
    int iov_cnt_;
    struct iovec iov_[2];

    Buffer read_buff_;          // 存储请求报文
    Buffer write_buff_;             // 存储响应报文

    HttpRequest request_;           
    HttpResponse response_;

    static bool is_Et_;
    static const char* src_dir_;
    static std::atomic_int user_count_;
};

#endif