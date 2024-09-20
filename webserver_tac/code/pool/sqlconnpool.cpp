#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool()
{
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool::~SqlConnPool()
{
    ClosePool();
}

SqlConnPool *SqlConnPool::Instance()
{
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char *host, int port, const char *user, const char *pwd, const char *dbName,
                       int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++)
    {
        MYSQL *sql = nullptr;
        // 初始化对象
        sql = mysql_init(sql);
        if (!sql)
        {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        // 建立连接
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (!sql)
        {
            LOG_ERROR("MySql Connect error!");
        }
        // 存入队列中
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    // 信号量初始化？
    sem_init(&semId_, 0, MAX_CONN_);
}

// 从队列中取出来一个连接
MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
        LOG_INFO("Get One SqlConnect From Pool");
    }
    return sql;
}


// 把连接放回去
void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);
    LOG_INFO("Free One SqlConnect To Pool");
}


void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();        
}

int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}