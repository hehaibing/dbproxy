#ifndef __DBPROXY_COMMON_H_
#define __DBPROXY_COMMON_H_

#include <string>
#include <libuv/uv.h>

namespace dbproxy{

class Client{
public:
    Client();
    void Init(uv_tcp_t* handle);
    void ProcessReaded();
    void Response(const std::string& content);

    void AllocReadBuffer(size_t suggested_size, uv_buf_t* buf);
    uv_stream_t* GetHandle(){   return (uv_stream_t*)handle_;};

    static void OnNewConnection(uv_stream_t* server, int status);
    static void OnAllocReadBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
    static void ReadCallback(uv_stream_t *client, ssize_t read_num, const uv_buf_t *buf);
    static void WriteCallback(uv_write_t* req, int status);
private:
    uv_tcp_t* handle_;
    char* read_buf_;
    int read_buf_len_;
    int alread_readed_num_;
};
}
#endif //__DBPROXY_COMMON_H_