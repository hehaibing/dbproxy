#include <hiredis/adapters/libuv.h>
#include "db_proxy_server.h"
#include "client.h"

namespace dbproxy{

const int DEFAULT_BACKLOG = 128;
DBProxyServer instance;

void DBProxyServer::Start(int listen_port){
	loop_ = uv_default_loop();
	redis_async_context_ = redisAsyncConnect("127.0.0.1", 6379);
    if (redis_async_context_->err) {
        fprintf(stderr,"Error: %s\n", redis_async_context_->errstr);
        return;
    }
    redisLibuvAttach(redis_async_context_,loop_);

	server_handle_ = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	uv_tcp_init(loop_,server_handle_);
	struct sockaddr_in addr;
	uv_ip4_addr("0.0.0.0", listen_port, &addr);
	uv_tcp_bind(server_handle_, (const struct sockaddr*)&addr, 0);
	int ret = uv_listen((uv_stream_t*) server_handle_, DEFAULT_BACKLOG, Client::OnNewConnection);
	if(ret){
		fprintf(stderr, "listen error %s\n", uv_strerror(ret));
		return;
	}
	uv_run(loop_,UV_RUN_DEFAULT);
}

uv_tcp_t* DBProxyServer::CreateClient(){
	uv_tcp_t *client_handle = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop_, client_handle);
    return client_handle;
}

redisAsyncContext* DBProxyServer::GetRedisContext(const std::string& player_id){
	return redis_async_context_;
}

DBProxyServer* DBProxyServer::Instance(){
	return &instance;
}

}