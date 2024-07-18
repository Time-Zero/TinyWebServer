#include "httpconn.h"
#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "../Log/log.h"

const char* HttpConn::src_dir_;
std::atomic_int HttpConn::user_count_;
bool HttpConn::is_Et_;

HttpConn::HttpConn() :
    fd_(-1),
    addr_({0}),
    is_close_(false)
{

}


HttpConn::~HttpConn(){
    Close();
}

const char* HttpConn::GetIp() const{
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const{
    return addr_.sin_port;
}

void HttpConn::Init(int fd, const sockaddr_in& addr){
    assert(fd > 0);
    user_count_.fetch_add(1);
    addr_ = addr;
    fd_ = fd;
    write_buff_.RetrieveAll();
    read_buff_.RetrieveAll();
    is_close_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIp(), GetPort(), user_count_.load());
}

// 关闭http处理
void HttpConn::Close(){
    response_.UnmapFile();
    if(is_close_ == false){
        is_close_ = true;
        user_count_.fetch_sub(1);
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIp(), GetPort(), user_count_.load());
    }
}

sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}

int HttpConn::GetFd() const{
    return fd_;
}

int HttpConn::ToWriteBytes(){
    return iov_[0].iov_len + iov_[1].iov_len;
}

// 解析请求报文，生成回应报文
bool HttpConn::process(){
    request_.Init();
    if(read_buff_.ReadableBytes() <= 0){            // 如果没有request报文需要解析
        return false;
    }else if(request_.Parse(read_buff_)){       // 如果解析成功
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(src_dir_, request_.path(), request_.IsKeepAlive(), 200);
    }else{      // 如果有报文需要解析，但是解析失败
        response_.Init(src_dir_, request_.path(), false, 400);
    }

    // 生成响应报文
    response_.MakeResponse(write_buff_);    

    iov_[0].iov_base = (char*)(write_buff_.Peek());     // 获取响应保存存储指针
    iov_[0].iov_len = write_buff_.ReadableBytes();      // 响应报文长度
    iov_cnt_ = 1;

    // 如果是纯纯一点文件都读不出来，这样会生成提示错误报文，这时候就不需要读取html，而是仅仅需要获取buffer中的内容即可
    // 如果响应需要读取html文件
    if(response_.GetFileLen() > 0 && response_.GetFile()){
        iov_[1].iov_base = response_.GetFile();
        iov_[1].iov_len = response_.GetFileLen();
        iov_cnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.GetFileLen() , iov_cnt_, ToWriteBytes());
    return true;
}

// 读取socket中的请求报文
ssize_t HttpConn::read(int* save_errno){
    ssize_t len = -1;
    do{
        len = read_buff_.ReadFd(fd_,save_errno);
        if(len <= 0)
            break;
    }while(is_Et_);         // is_Et:边沿触发，要一次性全部读取,因为ET触发同一事件只会触发一次，所以要在本次中读取完所有的报文

    return len;
}

// 将回应报文写入socket
ssize_t HttpConn::write(int* save_errno){
    ssize_t len = -1;
    do{
        len = writev(fd_, iov_, iov_cnt_);
        if(len <=0){
            *save_errno = errno;
            break;
        }

        if(iov_[0].iov_len + iov_[1].iov_len == 0) break;       // 传输结束
        else if ((size_t)len > iov_[0].iov_len){            // 如果写入长度大于iov[0]的内容长度，则说明iov[1]中的内容也被写入了一部分，所以需要动态调整
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);    // 移动读指针偏移量
            iov_[1].iov_len -= (len - iov_[0].iov_len);

            if(iov_[0].iov_len){
                write_buff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }else{      // 如果写入长度小于iov[0]的长度，则需要动态调整iov[0]的待写长度
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            write_buff_.Retrieve(len);
        }

    }while(is_Et_ || ToWriteBytes() > 10240);

    return len;
}

void HttpConn::SetSrcDir(const char* src_dir){
    src_dir_ = src_dir;
}

void HttpConn::SetSrcDir(const std::string& src_dir){
    SetSrcDir(src_dir.c_str());
}

void HttpConn::SetUserCount(const int user_count){
    user_count_ = user_count;
}

void HttpConn::SetIsEt(bool is_ET){
    is_Et_ = is_ET;
}