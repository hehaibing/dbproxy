#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "client.h"
#include "db_get.h"
#include "db_set.h"
#include "db_proxy_server.h"
#include "proto/dbproxy.pb.h"

namespace dbproxy{

const int READ_BUF_LEN = 1024;
const uint32_t MAX_READ_BUF_LEN = 1024*1024;

Client::Client()
    :handle_(NULL)
    ,read_buf_(NULL)
    ,read_buf_len_(0)
    ,alread_readed_num_(0){
}

Client::~Client(){
    if(read_buf_ != NULL){
        free(read_buf_);
        read_buf_ = NULL;
        alread_readed_num_ = 0;
    }
}

void Client::Init(uv_tcp_t* handle){
    handle_ = handle;
    if(read_buf_ != NULL){
        free(read_buf_);
    }
    read_buf_ = (char*)malloc(READ_BUF_LEN);
    read_buf_len_ = READ_BUF_LEN;
    alread_readed_num_ = 0;
}

void Client::ProcessReaded(){
    if(alread_readed_num_ < sizeof(MessageHeader)){
        return;
    }
    Message* msg = (Message*)read_buf_;
    uint32_t input_msg_len = ntohl(msg->head.content_len);
    
    if(input_msg_len > MAX_READ_BUF_LEN){
        handle_->data = this;
        uv_close((uv_handle_t*)handle_,Client::CloseCallback);
        return;
    }

    if(alread_readed_num_ < input_msg_len + sizeof(MessageHeader)){
        return;
    }
    msg->head.content_len = input_msg_len;
    if(msg->head.cmd == CMD_GET_REQ){
        DBGet::Process(msg,this);        
    } else if(msg->head.cmd == CMD_SET_REQ){
        DBSet::Process(msg,this);
    }
    alread_readed_num_ = 0;
}

void Client::AllocReadBuffer(size_t suggested_size, uv_buf_t* buf){
    if((size_t)(read_buf_len_ - alread_readed_num_) < suggested_size){
        int new_buf_len = suggested_size + alread_readed_num_;
        char* new_buf = (char*)malloc(new_buf_len);
        memcpy(new_buf,read_buf_,alread_readed_num_);
        free(read_buf_);
        read_buf_ = new_buf;
        read_buf_len_ = new_buf_len;
    }
    buf->base = read_buf_ + alread_readed_num_;
    buf->len = read_buf_len_ - alread_readed_num_;
}

void Client::OnNewConnection(uv_stream_t* server, int status){
    if(status < 0){
        fprintf(stderr,"New connection error %s\n",uv_strerror(status));
        return;
    }

    uv_tcp_t *client_handle = DBProxyServer::Instance()->CreateClient();
    if (uv_accept(server, (uv_stream_t*) client_handle) == 0) {
        Client* client = new Client();
        client->Init(client_handle);
        client_handle->data = client;
        uv_read_start((uv_stream_t*) client_handle, Client::OnAllocReadBuffer,Client::ReadCallback);
    }
    else {
        uv_close((uv_handle_t*) client_handle, Client::CloseCallback);
    }
}

void Client::OnAllocReadBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf){
    Client* client = (Client*)handle->data;
    client->AllocReadBuffer(suggested_size,buf);
}

void Client::ReadCallback(uv_stream_t *client_handle, ssize_t read_num, const uv_buf_t *buf){
    if(read_num < 0){
        if(read_num != UV_EOF){
            fprintf(stderr, "read error, %s\n", uv_strerror(read_num));
        }
        uv_close((uv_handle_t*)client_handle,Client::CloseCallback);
        return;
    }

    Client* client = (Client*)client_handle->data;
    client->alread_readed_num_ += read_num;
    client->ProcessReaded();
}

void Client::WriteCallback(uv_write_t* req, int status){
    if(req != NULL){
        free(req->data);
        free(req);
    }
}

void Client::Response(uint32_t cmd, uint32_t sn, const std::string& content){
    uint32_t msg_len = content.length()+sizeof(MessageHeader);
    Message* msg = (Message*)malloc(msg_len);
    msg->head.content_len = htonl(content.length());
    msg->head.sn = sn;
    msg->head.cmd = cmd;
    memcpy(msg->content,content.c_str(),content.length());

    uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    write_req->data = msg;
    uv_buf_t buf = uv_buf_init((char*)msg,msg_len);
    uv_write(write_req,(uv_stream_t*)handle_,&buf,1,Client::WriteCallback);
}

void Client::CloseCallback(uv_handle_t* handle){
    if(handle->data != NULL){
        Client* client = (Client*)handle->data;
        delete client;
    }
    free(handle);
}

}