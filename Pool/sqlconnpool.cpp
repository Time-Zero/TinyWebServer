#include "sqlconnpool.h"
#include "../Log/log.h"
#include "mysql.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

SqlConnPool::SqlConnPool():
    is_close_(false)
{

}

SqlConnPool& SqlConnPool::Instance(){
    static SqlConnPool ins;
    return ins;
}

void SqlConnPool::Init(const std::string host, const unsigned int port, 
            const std::string user, const std::string password,
            const std::string dbname, int max_conn_size)
{
    std::unique_lock<std::mutex> lck(mtx_);

    assert(max_conn_size > 0);

    for(int i = 0 ; i < max_conn_size; i++){
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if(!conn){
            LOG_ERROR("Mysql Init Error!");
            assert(conn);
        }

        conn = mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0);
        if(!conn){
            LOG_ERROR("Mysql Connect Error!");
        }

        conn_queue_.emplace(conn);
    }
}

MYSQL* SqlConnPool::GetConn(){
    std::unique_lock<std::mutex> lck(mtx_);
    while(conn_queue_.empty()){
        if(cv_con_.wait_for(lck, std::chrono::microseconds(TIMEOUT_COUNT)) == std::cv_status::timeout){
            return nullptr;
        }

        if(is_close_.load())
            return nullptr;
    }

    MYSQL* conn = conn_queue_.front();
    conn_queue_.pop();
    return conn;
}

void SqlConnPool::FreeConn(MYSQL* conn){
    assert(conn);
    std::lock_guard<std::mutex> lck(mtx_);
    
    if(is_close_.load())
        mysql_close(conn);

    conn_queue_.emplace(conn);
    cv_con_.notify_one();
}

void SqlConnPool::CloseSqlConnPool(){
    std::lock_guard<std::mutex> lck(mtx_);
    
    while(!conn_queue_.empty()){
        auto conn =  conn_queue_.front();
        conn_queue_.pop();
        mysql_close(conn);
    }
}

int SqlConnPool::GetFreeConnCount(){
    std::lock_guard<std::mutex> lck(mtx_);
    return conn_queue_.size();
}