#ifndef __DBPROXY_DB_PROXY_H__
#define __DBPROXY_DB_PROXY_H__
#include <string>

#include <hiredis/async.h>
#include <libuv/uv.h>
#include <mysql_connection.h>


namespace dbproxy
{

class DBProxyServer
{
public:
    static DBProxyServer* Instance();
    void Start(int listen_port);

    uv_tcp_t* CreateClient();
    redisAsyncContext* GetRedisContext(const std::string& player_id);
    sql::Connection* GetMysqlConnection(const std::string& player_id);
    uv_loop_t* Loop(){return loop_;};
private:
    DBProxyServer():server_handle_(NULL),loop_(NULL),redis_async_context_(NULL){};
    uv_tcp_t *server_handle_;
    uv_loop_t *loop_;
    redisAsyncContext *redis_async_context_;
};

}
#endif //__DBPROXY_DB_PROXY_H__