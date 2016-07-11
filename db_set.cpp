
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "db_set.h"
#include "client.h"
#include "db_proxy_server.h"
#include "proto/dbproxy.pb.h"

namespace dbproxy{

const char* INSERT_SQL = "insert into players(player_id,prop_name,prop_value) values(?,?,?) on duplicate key update prop_value = values(prop_value)";

static void ResponseToClient(Client* client, uint32_t sn, SetResp& response){
    std::string serialData;
    response.SerializeToString(&serialData);
    client->Response(CMD_SET_RESP,sn,serialData);
}

void DBSet::Process(Message *message,Client* client){
    SetReq req;
    google::protobuf::io::ArrayInputStream inputStream(message->content,message->head.content_len);
    if(!req.ParseFromZeroCopyStream(&inputStream)){
        SetResp resp;
        resp.set_ret_code(ERR_MESSAGE_FORMAT);
        resp.set_error_message("protobuf parse failed");
        ResponseToClient(client,message->head.sn,resp);
        return;
    }

    redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(req.player_id());
    if(redis_context == NULL){
        SetResp resp;
        resp.set_ret_code(ERR_DB_EXCEPTION);
        resp.set_error_message("redis connection lost");
        ResponseToClient(client,message->head.sn,resp);
        return;
    }

    DBSetCommand* command = new DBSetCommand();
    command->client = client;
    command->sn = message->head.sn;
    command->player_id = req.player_id();
    command->prop_name = req.prop_name();
    command->prop_value = req.prop_value();
    redisAsyncCommand(redis_context,DBSet::SetRedisCallback,command,
        "hset %s %s %b",
        command->player_id.c_str(),
        command->prop_name.c_str(),
        command->prop_value.c_str(),
        command->prop_value.length());
}

void DBSet::SetRedisCallback(redisAsyncContext *async_context, void *reply, void *privdata){
    DBSetCommand* command = (DBSetCommand*)privdata;
    redisReply *r = (redisReply*)reply;
    SetResp resp;
    if(reply == NULL){
        resp.set_ret_code(ERR_DB_EXCEPTION);
        resp.set_error_message("redis reply is null");
        ResponseToClient(command->client,command->sn,resp);
        return;
    }

    if(r->type != REDIS_REPLY_INTEGER){
        resp.set_ret_code(ERR_SET_REDIS_FAILED);
        resp.set_error_message(r->str,r->len);
        ResponseToClient(command->client,command->sn,resp);
        return;
    }

    resp.set_ret_code(ERR_SUCCESS);
    ResponseToClient(command->client,command->sn,resp);

    uv_work_t* work_handle = (uv_work_t*)malloc(sizeof(uv_work_t));
    work_handle->data = command;
    uv_loop_t* loop = DBProxyServer::Instance()->Loop();
    uv_queue_work(loop,work_handle,DBSet::InsertMysql,DBSet::InsertMysqlCallback);
}

void DBSet::InsertMysql(uv_work_t *work_handle){
    DBSetCommand* command = (DBSetCommand*) work_handle->data;
    sql::Connection* conn = DBProxyServer::Instance()->GetMysqlConnection(command->player_id);
    if(conn == NULL){
        command->insert_result = DBSetCommand::FAILED;
        return;
    }
    try{        
        std::auto_ptr<sql::PreparedStatement> ps (conn->prepareStatement(INSERT_SQL));
        ps->setString(1,command->player_id);
        ps->setString(2,command->prop_name);
        ps->setString(3,command->prop_value);
        ps->execute();
        command->insert_result = DBSetCommand::SUCCESS;
    }catch(sql::SQLException& e){
        command->insert_result = DBSetCommand::FAILED;
        std::cerr << "failed to execute sql in mysql, error is "<<e.what()
                  <<", mysql error code is "<<e.getErrorCode()<<std::endl;
    }
}

void DBSet::InsertMysqlCallback(uv_work_t *work_handle, int status){
    //ignore
}

}