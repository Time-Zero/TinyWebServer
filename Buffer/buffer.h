#ifndef BUFFER_H
#define BUFFER_H

#include <atomic>
#include <cstddef>
#include <string>
#include <sys/types.h>
#include <vector>
#include <sys/uio.h>
#include <unistd.h>

class Buffer{
public:
    Buffer(unsigned int buffer_size = 1024);
    ~Buffer() = default;

    std::size_t WritableBytes() const;
    std::size_t ReadableBytes() const;
    std::size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWritable(const size_t len);
    void HasWritten(const size_t len);
    void Retrieve(const size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();
    std::string RetrieveAllToStr();
    const char* BeginWriteConst() const;
    char* BeginWrite();
    void Append(const char* str, size_t len);
    void Append(const std::string& str);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buffer);
    ssize_t ReadFd(int fd, int *err);
    ssize_t WriteFd(int fd, int *err);

private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(std::size_t len);

private:
    std::vector<char> buffer_;          
    std::atomic<std::size_t> read_pos_;
    std::atomic<std::size_t> write_pos_;
};

#endif