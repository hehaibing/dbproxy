#include <hiredis/adapters/libuv.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>

#include "db_proxy_server.h"
#include "client.h"

namespace dbproxy{

const int DEFAULT_BACKLOG = 128;
static uv_key_t g_mysql_connection_key;

DBProxyServer::~DBProxyServer(){ 
    if(server_handle_ != NULL){
        free(server_handle_);
        server_handle_ = NULL;
    }
}

void handle_signal_int(uv_signal_t* handle, int signum){
    uv_signal_stop(handle);
    uv_close((uv_handle_t*)handle, NULL);
    DBProxyServer::Instance()->Stop();
}

static void walk_cb(uv_handle_t* handle, void* arg){
    printf("handle type is %d\n", handle->type);
}

static void close_cb(uv_handle_t* handle) {
    printf("close callback execute\n");
    uv_walk(uv_default_loop(),walk_cb,NULL);
}

static void shutdown_cb(uv_shutdown_t* req, int status) {
    printf("shutdown callback\n");
    uv_close((uv_handle_t*)req->data, close_cb);
}

void DBProxyServer::Stop(){
    printf("begin to shutdown\n");
    if(redis_async_context_ != NULL) {
        redisAsyncDisconnect(redis_async_context_);
    }
    uv_close((uv_handle_t*)server_handle_,NULL);    
}

void DBProxyServer::Start(int listen_port){
    loop_ = uv_default_loop();
    int key_create_ret = uv_key_create(&g_mysql_connection_key);

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
    uv_signal_init(loop_,&signal_handle_);
    uv_signal_start(&signal_handle_,handle_signal_int,SIGINT);
    uv_run(loop_,UV_RUN_DEFAULT);
    uv_loop_close(loop_);
}

uv_tcp_t* DBProxyServer::CreateClient(){
    uv_tcp_t *client_handle = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop_, client_handle);
    client_handle->data = NULL;
    return client_handle;
}

redisAsyncContext* DBProxyServer::GetRedisContext(const std::string& player_id){
    return redis_async_context_;
}

sql::Connection* DBProxyServer::GetMysqlConnection(const std::string& player_id){
    sql::Connection* conn = (sql::Connection*)uv_key_get(&g_mysql_connection_key);
    if(conn !=  NULL){
        return conn;
    }
    try{
        sql::Driver* driver = get_driver_instance();
        conn = driver->connect("tcp://127.0.0.1:3306/dbproxy","root","root");
        uv_key_set(&g_mysql_connection_key,conn);       
    }catch (sql::SQLException &e){
        std::cerr << "failed to connect mysql, error is "<<e.what()
                  <<"mysql error code is "<<e.getErrorCode();
    }
    return conn;
}

DBProxyServer* DBProxyServer::Instance(){
    static DBProxyServer instance;
    return &instance;
}

}