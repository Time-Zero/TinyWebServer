#include "epoller.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sys/epoll.h>
#include <unistd.h>

Epoll::Epoll(int max_event) :
    epoll_fd_(epoll_create(512)),
    events_(max_event)
{
    assert(epoll_fd_ >= 0 && events_.size() > 0);
}

Epoll::~Epoll(){
    close(epoll_fd_);
}

bool Epoll::AddFd(int fd, uint32_t events){
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

bool Epoll::ModFd(int fd, uint32_t events){
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

bool Epoll::DelFd(int fd, uint32_t events){
    if(fd < 0) return false;

    return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, 0);
}

int Epoll::Wait(int time_out_ms){
    return epoll_wait(epoll_fd_, &events_[0], (int)events_.size(), time_out_ms);
}

int Epoll::GetEventFd(size_t i) const {
    assert(i >= 0 && events_.size() > i);
    return events_[i].data.fd;
}

uint32_t Epoll::GetEvents(size_t i) const {
    assert(i >= 0 && events_.size() > i);
    return events_[i].events;
}