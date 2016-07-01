#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "client.h"
#include "db_get.h"
#include "db_proxy_server.h"

namespace dbproxy{

const int READ_BUF_LEN = 1024;
const int WRITE_BUF_LEN = 1024;

Client::Client()
	:handle_(NULL)
	,read_buf_(NULL)
	,read_buf_len_(0)
	,alread_readed_num_(0)
	,write_buf_(NULL)
	,write_buf_len_(0){

}

void Client::Init(uv_tcp_t* handle){
	handle_ = handle;
	if(read_buf_ != NULL){
		free(read_buf_);
	}
	if(write_buf_ != NULL){
		free(write_buf_);
	}
	read_buf_ = (char*)malloc(READ_BUF_LEN);
	read_buf_len_ = READ_BUF_LEN;
	alread_readed_num_ = 0;

	write_buf_ = (char*)malloc(WRITE_BUF_LEN);
	write_buf_len_ = WRITE_BUF_LEN;
}

void Client::ProcessReaded(){
	if(alread_readed_num_ <= 2){
		return;
	}
	//uint16_t input_msg_len = ntohs(*(uint16_t*)read_buf_);
	uint16_t input_msg_len = (uint16_t)strtol(read_buf_,NULL,10);
	if(alread_readed_num_ < input_msg_len+2){
		return;
	}

	DBGetCommand* command = new DBGetCommand();
	command->client = this;
	command->player_id = std::string(read_buf_+2,alread_readed_num_-2);
	command->key = "fix";
	alread_readed_num_ = 0;
	DBGet::Process(command);
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

void Client::AllocWriteBuffer(size_t suggested_size, uv_buf_t *buf){
	if((size_t)write_buf_len_ < suggested_size){
		free(write_buf_);
		write_buf_ = (char*)malloc(suggested_size);
		write_buf_len_ = suggested_size;
	}
	buf->base = write_buf_;
	buf->len = write_buf_len_;
}

void Client::OnNewConnection(uv_stream_t* server, int status){
	if(status < 0){
        fprintf(stderr,"New connection error %s\n",uv_strerror(status));
        return;
    }

    uv_tcp_t *client_handle = DBProxyServer::Instance()->CreateClient();
    if (uv_accept(server, (uv_stream_t*) client_handle) == 0) {
        fprintf(stdout,"Accept new client\n");
        Client* client = new Client();
        client->Init(client_handle);
        client_handle->data = client;
        uv_read_start((uv_stream_t*) client_handle, Client::OnAllocReadBuffer,Client::ReadCallback);
    }
    else {
        fprintf(stderr,"Accept client error\n");
        uv_close((uv_handle_t*) client_handle, NULL);
    }
}

void Client::OnAllocReadBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf){
	Client* client = (Client*)handle->data;
	client->AllocReadBuffer(suggested_size,buf);
}

void Client::ReadCallback(uv_stream_t *client_handle, ssize_t read_num, const uv_buf_t *buf){
	Client* client = (Client*)client_handle->data;
	client->alread_readed_num_ += read_num;
	client->ProcessReaded();
}

void Client::WriteCallback(uv_write_t* req, int status){
	free(req);
}

}