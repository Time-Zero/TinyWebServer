#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "../Timer/heaptimer.h"
#include "../Epoller/epoller.h"
#include "../Http/httpconn.h"


#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>

constexpr int MAX_FD = 65535;

class WebServer{
public:
    WebServer(int port, int trig_mode, int time_out_ms, bool opt_linger,
            int sql_port, const char* sql_user, const char* sql_pwd, const char* db_name, int conn_pool_num, 
            bool open_log, int log_level, int log_que_size);

    ~WebServer();
    void Start();

private:
    void InitEventMode(int trig_mode);
    bool InitSocket();
    void DealListen();
    void SendError(int fd, const char* info);
    void CloseConn(HttpConn* client);
    void AddClient(int fd, sockaddr_in addr);
    void ExtendTime(HttpConn* client);
    int SetFdNoBlock(int fd);
    void OnProcess(HttpConn* client);
    void OnWrite(HttpConn* client);
    void OnRead(HttpConn* client);
    void DealRead(HttpConn* client);
    void DealWrite(HttpConn* client);


private:
    int port_;
    bool opt_linger_;
    int time_out_ms_;
    bool is_close_;
    int listen_fd_;
    char* src_dir_;

    uint32_t listen_event_;     // 连接监听端口
    uint32_t conn_event_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};

#endif