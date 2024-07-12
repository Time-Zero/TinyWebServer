#include "buffer.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <sys/types.h>
#include <sys/uio.h>

Buffer::Buffer(unsigned int buffer_size) : 
buffer_(buffer_size) ,
read_pos_(0),
write_pos_(0)
{

}

// 剩余可以写的大小
std::size_t Buffer::WritableBytes() const{
    return buffer_.size() - write_pos_;
}

// 可以读取的大小
std::size_t Buffer::ReadableBytes() const{
    return write_pos_ - read_pos_;
}

// 已经读取过的大小，也就是可以回收的大小
std::size_t Buffer::PrependableBytes() const{
    return read_pos_;
}

// 缓冲区头指针
char* Buffer::BeginPtr_(){
    return &buffer_[0];
}

const char* Buffer::BeginPtr_() const{
    return &buffer_[0];
}

// 为缓冲区创造空间，如果空间还够，就平移数据。如果空间不够，就重新分配空间
void Buffer::MakeSpace_(std::size_t len){
    if(WritableBytes() + PrependableBytes() < len){
        buffer_.resize(write_pos_ + len + 1);
    }else{
        size_t readable_bytes = ReadableBytes();
        std::copy(BeginPtr_() + read_pos_, BeginPtr_() + write_pos_ , BeginPtr_());
        read_pos_ = 0;
        write_pos_ = readable_bytes;
    }
}

// 返回可以读取位置的指针
const char* Buffer::Peek() const{
    return &buffer_[read_pos_];
}

// 确保有足够写的空间
void Buffer::EnsureWritable(size_t const len){
    if(len > WritableBytes())
        MakeSpace_(len);
    
    assert(len < WritableBytes());
}

// 写指针移动指定位置
void Buffer::HasWritten(const size_t len){
    write_pos_ += len;
}

// 读指针移动指定长度
void Buffer::Retrieve(const size_t len){
    read_pos_ += len;
}

// 读指针移动到指定位置
void Buffer::RetrieveUntil(const char* end){
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

// 回收全部空间
void Buffer::RetrieveAll(){
    buffer_.clear();
    read_pos_ = write_pos_ = 0;
}

// 读取所有待读取空间，并且以string的方式返回
std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针的位置
const char* Buffer::BeginWriteConst() const{
    return &buffer_[write_pos_];
}

char* Buffer::BeginWrite(){
    return &buffer_[write_pos_];
}

// buffer中插入数据
void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWritable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const std::string& str){
    Append(str.c_str(), str.size());
}

void Buffer::Append(const void* data, size_t len){
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const Buffer& buffer){
    Append(buffer.Peek(), buffer.ReadableBytes());
}

// 从文件描述符中读取字符到buffer中
ssize_t Buffer::ReadFd(int fd, int *err){
    char buffer[65535];
    struct iovec iov[2];
    size_t writable_bytes = WritableBytes();

    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writable_bytes;
    iov[1].iov_base = buffer;
    iov[1].iov_len = sizeof(buffer);

    ssize_t len = readv(fd, iov, 2);
    if(len < 0){
        *err = errno;
    }else if((size_t)len <= writable_bytes){
        HasWritten(len);
    }else{
        write_pos_ = buffer_.size();
        Append(buffer, (size_t)(len - writable_bytes));
    }

    return len;
}

// 将buffer中的内容写入文件
ssize_t Buffer::WriteFd(int fd, int *err){
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if(len < 0){
        *err = errno;
    }
    Retrieve(len);
    return len;
}

