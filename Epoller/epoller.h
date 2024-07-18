#ifndef EPOLL_H
#define EPOLL_H

#include <cstddef>
#include <cstdint>
#include <sys/epoll.h>
#include <sys/types.h>
#include <vector>

class Epoll{
public:
    explicit Epoll(int max_event=1024);
    ~Epoll();

    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd, uint32_t events);
    int Wait(int time_out_ms = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;

private:
    int epoll_fd_;
    std::vector<struct epoll_event> events_;
};

#endif