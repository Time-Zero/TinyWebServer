#include "webserver.h"
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../Pool/sqlconnpool.h"
#include <arpa/inet.h>
#include "../Log/log.h"
#include "../Pool/threadpool.h"


WebServer::WebServer(int port, int trig_mode, int time_out_ms, bool opt_linger,
            int sql_port, const char* sql_user, const char* sql_pwd, const char* db_name, int conn_pool_num, 
            bool open_log, int log_level, int log_que_size) :
            port_(port), opt_linger_(opt_linger), time_out_ms_(time_out_ms), is_close_(false),
            timer_(new HeapTimer), epoller_(new Epoller), src_dir_(nullptr)
{

     // 是否打开日志
    if(open_log){
        Log::instance().init(log_level, "./log",".log",log_que_size);

        if(is_close_){
            LOG_ERROR("========== Server init error!==========");
        }
        else{
            LOG_INFO("============Server Init ===============");
            LOG_INFO("Port: %d, OptLinger: %s", port_, opt_linger_ ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", (listen_event_ & EPOLLET ? "ET" : "LT"), (conn_event_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("Log Level is: %d", log_level);
            LOG_INFO("SrcDir: %s", src_dir_);
            LOG_INFO("SqlConnPool Num: %d", conn_pool_num);
        }
    }

    // 前端文件存放位置
    src_dir_ = getcwd(nullptr, 256);
    assert(src_dir_);
    strcat(src_dir_, "/resources/");

    // 设置前端文件位置
    HttpConn::SetSrcDir(src_dir_);
    HttpConn::SetUserCount(0);

    // 初始化连接池
    SqlConnPool::Instance().Init("localhost", sql_port, sql_user, sql_pwd, db_name, conn_pool_num);
    
    // 设置事件触发模式
    InitEventMode(trig_mode);

    // 初始化监听socket
    if(!InitSocket())       // 如果初始化失败，直接关闭程序
        is_close_ = true;

}

WebServer::~WebServer(){
    close(listen_fd_);
    is_close_ = true;
    free(src_dir_);
    SqlConnPool::Instance().CloseSqlConnPool();
}

void WebServer::InitEventMode(int trig_mode){
    listen_event_ = EPOLLRDHUP;         // listen_event先赋值为关闭连接
    conn_event_ = EPOLLONESHOT | EPOLLRDHUP;        // 设置关闭连接，并且设置同一事件不要多次通知，仅仅使用单个线程处理这个事件

    // 设置什么事件采用ET触发（如果不设置ET触发， 默认就是LT触发）
    switch (trig_mode) {
        case 0 :
            break;
        case 1 :
            conn_event_ |= EPOLLET;
            break;
        case 2:
            listen_event_ |= EPOLLET;
            break;
        case 3:
            listen_event_ |= EPOLLET;
            conn_event_ |= EPOLLET;
            break;
        default:
            listen_event_ |= EPOLLET;
            conn_event_ |= EPOLLET;
            break;
    }


    HttpConn::SetIsEt(conn_event_ | EPOLLET);
}

// 初始化socket
bool WebServer::InitSocket(){
    
    if(port_ > 65535 || port_ < 1024){      // 检查端口号是不是在合理范围内
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    
    int ret;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    
    
    // 设置套接字在关闭时的处理方法
    struct linger optLinger = {0};          // linger用来处理在关闭socket时，内核如何处理还未发送完毕的数据
    if(opt_linger_){
        optLinger.l_onoff = 1;              // 关闭套机字时阻塞，并尝试发送未发送完的数据
        optLinger.l_linger = 1;         // 设置这个发送行为的超时时间为1s
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd_ < 0){
        LOG_ERROR("Create socket error! Port is: %d", port_);
        return false;
    }
    
    ret = setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0){
        close(listen_fd_);
        LOG_ERROR("Init linger error! Port is: %d", port_);
        return false;
    }

    // 设置端口复用
    int optval = 1;
    ret = setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret < 0){
        LOG_ERROR("set socket setsockopt error !");
        close(listen_fd_);
        return false;
    }

    ret = bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listen_fd_);
        return false;
    }

    // 监听socket，最大等待连接长度为6
    ret = listen(listen_fd_, 6);
    if(ret < 0){
        LOG_ERROR("Listen port:%d error!", port_);
        close(listen_fd_);
        return false;
    }


    // 把设置好的端口放入epoll中，并且只有读事件通知
    ret = epoller_->AddFd(listen_fd_, listen_event_ | EPOLLIN);
    if(ret == false){
        LOG_ERROR("Add listen error!");
        close(listen_fd_);
        return false;
    }

    // 设置socket为非阻塞模式
    SetFdNoBlock(listen_fd_);
    LOG_INFO("Server port: %d", port_);
    return true;
}

// 设置socket为非阻塞
int WebServer::SetFdNoBlock(int fd){
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);       // 获取当前socket文件描述符标志，并且添加非阻塞标志
}

// 发送错误，使用send方法
void WebServer::SendError(int fd, const char* info){
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0){
        LOG_WARN("send error to client[%d] error!", fd);
    }

    close(fd);
}

void WebServer::CloseConn(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close(); 
}

// 添加socket到小根堆和epoll中
void WebServer::AddClient(int fd, sockaddr_in addr){
    assert(fd > 0);
    users_[fd].Init(fd, addr);              
    if(time_out_ms_ > 0){
        timer_->Add(fd, time_out_ms_, std::bind(&WebServer::CloseConn, this, &users_[fd]));
    }

    epoller_->AddFd(fd, EPOLLIN | conn_event_);     // 加入到epoll中，注册事件为IN
    SetFdNoBlock(fd);       // 设置socket为非阻塞
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听socket
void WebServer::DealListen(){
    struct sockaddr_in addr;        // 声明一个addr
    socklen_t len = sizeof(addr);

    do{
        int fd = accept(listen_fd_, (struct sockaddr*)&addr, &len);     // 接受这个socket
        if(fd <= 0) return;         
        else if(HttpConn::GetUserCount() >= MAX_FD){
            SendError(fd, "Server Busy");
            LOG_WARN("Clients is Full!");
            return;
        }
        AddClient(fd, addr);        // 否则将这个socket放入监听队列中
    }while(listen_event_ & EPOLLET);
}

void WebServer::Start(){
    int time_ms = -1;       
    if(!is_close_){
        LOG_INFO("==============Server Start================");
        while(!is_close_){
            if(time_out_ms_ > 0){
                time_ms = timer_->GetNextTick();        // 获取下一个超时等待时间
            }

            int eventCnt = epoller_->Wait(time_ms);     // 超时事件设置为下一个超时事件的时间
            for(int i = 0 ; i < eventCnt; i++){
                int fd = epoller_->GetEventFd(i);       // 获取事件中文件描述符
                uint32_t events = epoller_->GetEvents(i);       // 获取事件的事件类型
                
                if(fd == listen_fd_){       // 如果是我们的监听socket
                    DealListen();
                }else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){      // 如果是关闭或者错误
                    assert(users_.count(fd) > 0);
                    CloseConn(&users_[fd]);     // 关闭socket
                }else if(events & EPOLLIN){
                    assert(users_.count(fd) > 0);
                    DealRead(&users_[fd]);      // 处理读事件
                }else if(events & EPOLLOUT){
                    assert(users_.count(fd) > 0);
                    DealWrite(&users_[fd]);
                }else {
                    LOG_ERROR("Unexpected Event");
                }
            }
        }
    }
}

void WebServer::ExtendTime(HttpConn* client){
    assert(client);
    if(time_out_ms_ > 0) {timer_->Adjust(client->GetFd(), time_out_ms_);}
}

// 处理报文
void WebServer::OnProcess(HttpConn* client){
    if(client->process()){      // 解析请求报文，并且生成响应报文
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);       // 设置事件为OUT
    }else{
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);        // 如果没有要处理的报文，就为IN
    }
}

// 处理读取
void WebServer::OnRead(HttpConn* client){
    assert(client);
    int ret = -1;
    int read_errno = 0;
    ret = client->read(&read_errno);        // 读取请求报文
    if(ret <= 0 && read_errno != EAGAIN){       
        CloseConn(client);
        return;
    }

    OnProcess(client);
}

// 处理写事件
void WebServer::OnWrite(HttpConn* client){
    assert(client);
    int ret = -1;
    int write_errno = 0;

    ret = client->write(&write_errno);  // 将响应报文写入
    if(client->ToWriteBytes() == 0){        // 如果写完了
        if(client->IsKeepAlive()){      // 并且socket设置的是keepalive
            epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);    // 再设置为IN
            return;
        }
    }else if(ret < 0){      // 如果响应报文没写完
        if(write_errno == EAGAIN){      // 并且异常为EAGAIN
            epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);       // 再次设置为OUT，等待下次写入
            return;
        }
    }

    CloseConn(client);
}

// 处理socket的读事件
void WebServer::DealRead(HttpConn* client){
    assert(client);
    ExtendTime(client);         // 延长socket的超时时间    
    ThreadPool::Instance().commit(std::bind(&WebServer::OnRead, this, client));     // 在线程池中处理读取
}

// 处理socket的写事件
void WebServer::DealWrite(HttpConn* client){
    assert(client);
    ExtendTime(client);
    ThreadPool::Instance().commit(std::bind(&WebServer::OnWrite,this,client));
}