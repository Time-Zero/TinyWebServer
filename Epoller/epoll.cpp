#include "epoller.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sys/epoll.h>
#include <unistd.h>

Epoller::Epoller(int max_event) :
    epoll_fd_(epoll_create(512)),
    events_(max_event)
{
    assert(epoll_fd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller(){
    close(epoll_fd_);
}

bool Epoller::AddFd(int fd, uint32_t events){
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0){
        return true;
    }else{
        return false;
    }
}

bool Epoller::ModFd(int fd, uint32_t events){
    if(fd < 0)  return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    if(epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0){
        return true;
    }else{
        return false;
    }
}

bool Epoller::DelFd(int fd){
    if(fd < 0) return false;

    return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, 0);
}

int Epoller::Wait(int time_out_ms){
    return epoll_wait(epoll_fd_, &events_[0], (int)events_.size(), time_out_ms);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i >= 0 && events_.size() > i);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i >= 0 && events_.size() > i);
    return events_[i].events;
}