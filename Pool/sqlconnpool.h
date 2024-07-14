#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H
#include "mysql.h"
#include "nocopy.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>


constexpr int TIMEOUT_COUNT=100;

class SqlConnPool : public NoCopy{
public:
    static SqlConnPool& Instance();

    void Init(const std::string host, const unsigned int port, 
            const std::string user, const std::string password,
            const std::string dbname, int max_conn_size = 10);

    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);
    void CloseSqlConnPool();
    int GetFreeConnCount();

private:
    SqlConnPool();
    ~SqlConnPool(){
        mysql_library_end();
    }
private:
    std::atomic_bool is_close_;
    std::queue<MYSQL* > conn_queue_;
    std::mutex mtx_;
    std::condition_variable cv_con_;
};


#endif