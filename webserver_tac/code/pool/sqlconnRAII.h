#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H

#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
// 即在构造函数，取出一个conn，在析构函数，再把conn放回去
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};


#endif