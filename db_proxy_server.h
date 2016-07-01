#ifndef __DBPROXY_DB_PROXY_H__
#define __DBPROXY_DB_PROXY_H__
#include <hiredis/async.h>
#include <libuv/uv.h>
#include <string>

namespace dbproxy
{

class DBProxyServer
{
public:
	DBProxyServer():server_handle_(NULL),loop_(NULL),redis_async_context_(NULL){};
	void Start(int listen_port);
	uv_tcp_t* CreateClient();
	redisAsyncContext* GetRedisContext(const std::string& player_id);
	static DBProxyServer* Instance();	
private:
	uv_tcp_t *server_handle_;
	uv_loop_t *loop_;
	redisAsyncContext *redis_async_context_;
};

}
#endif //__DBPROXY_DB_PROXY_H__