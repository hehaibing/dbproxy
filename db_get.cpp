#include <stdlib.h>
#include <string.h>
#include <memory>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
//Mysql include
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "db_get.h"
#include "client.h"
#include "db_proxy_server.h"
#include "proto/dbproxy.pb.h"

namespace dbproxy
{

static void ResponseToClient(Client* client, uint32_t sn, GetResp& response){
    std::string serialData;
    response.SerializeToString(&serialData);
    client->Response(CMD_GET_RESP,sn,serialData);
}

void DBGet::Process(Message *message,Client *client){
    GetReq req;
    google::protobuf::io::ArrayInputStream inputStream(message->content,message->head.content_len);
    if(!req.ParseFromZeroCopyStream(&inputStream)){
        GetResp resp;
        resp.set_ret_code(ERR_MESSAGE_FORMAT);
        resp.set_error_message("protobuf parse failed");
        ResponseToClient(client,message->head.sn,resp);
        return;
    }

    redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(req.player_id());
    if(redis_context == NULL){
        GetResp resp;
        resp.set_ret_code(ERR_DB_EXCEPTION);
        resp.set_error_message("redis connection lost");
        ResponseToClient(client,message->head.sn,resp);
        return;
    }
    
    DBGetCommand* command = new DBGetCommand();
    command->client = client;
    command->sn = message->head.sn;
    command->player_id = req.player_id();
    command->prop_name = req.prop_name();
    redisAsyncCommand(redis_context,DBGet::GetRedisCallback,command,
        "hget %s %s",command->player_id.c_str(),command->prop_name.c_str());
}

void DBGet::GetRedisCallback(redisAsyncContext *context, void *reply, void *privdata){
    redisReply *redis_replay = (redisReply*)reply;
    DBGetCommand* command = (DBGetCommand*)privdata;    
    if(redis_replay->type == REDIS_REPLY_STRING){        
        GetResp resp;
        resp.set_ret_code(ERR_SUCCESS);
        resp.set_data(std::string(redis_replay->str,redis_replay->len));
        ResponseToClient(command->client,command->sn,resp);
        delete command;
        return;
    }

    //Lookup from mysql
    uv_work_t* work_handle = (uv_work_t*)malloc(sizeof(uv_work_t));
    work_handle->data = command;
    uv_loop_t* loop = DBProxyServer::Instance()->Loop();
    uv_queue_work(loop,work_handle,DBGet::QueryMysql,DBGet::QueryMysqlCallback);
}

void DBGet::SetRedisCallback(redisAsyncContext *context, void *reply, void *privdata){
    //do nothing, retry next time while failed
}

//important!! execute in diffent thread
void DBGet::QueryMysql(uv_work_t *work_handle){
    printf("query from mysql \n");
    DBGetCommand* command = (DBGetCommand*) work_handle->data;
    sql::Connection* conn = DBProxyServer::Instance()->GetMysqlConnection(command->player_id);
    if(conn == NULL){
        command->query_result = DBGetCommand::QUERY_FAILED;
        return;
    }
    try{
        std::auto_ptr<sql::PreparedStatement> ps (conn->prepareStatement("select prop_value from players where player_id = ? and prop_name = ?"));
        ps->setString(1,command->player_id);
        ps->setString(2,command->prop_name);

        std::auto_ptr<sql::ResultSet> rs (ps->executeQuery());
        if(rs->next()){
            command->query_result = DBGetCommand::FOUNDED;
            command->prop_value = rs->getString(1);
        }else{
            command->query_result = DBGetCommand::NOT_FOUNDED;
        }
    }catch(sql::SQLException& e){
        command->query_result = DBGetCommand::QUERY_FAILED;
        std::cerr << "failed to execute sql in mysql, error is "<<e.what()
                  <<", mysql error code is "<<e.getErrorCode()<<std::endl;
    }
}

void DBGet::QueryMysqlCallback(uv_work_t* work_handle, int status){
    GetResp resp;
    DBGetCommand* command = (DBGetCommand*) work_handle->data;    
    if(command->query_result == DBGetCommand::FOUNDED){
        resp.set_ret_code(ERR_SUCCESS);
        resp.set_data(command->prop_value);
    }else if(command->query_result == DBGetCommand::NOT_FOUNDED){
        resp.set_ret_code(ERR_GET_NOT_FOUND);
        resp.set_error_message("can not find in the database");
    }else{
        resp.set_ret_code(ERR_DB_EXCEPTION);
        resp.set_error_message("query database failed");
    }
    ResponseToClient(command->client,command->sn,resp);

    redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(command->player_id);
    if(command->query_result == DBGetCommand::FOUNDED && redis_context != NULL){
        redisAsyncCommand(redis_context,DBGet::SetRedisCallback,command,
            "hset %s %s %b",
            command->player_id.c_str(),
            command->prop_name.c_str(),
            command->prop_value.c_str(),
            command->prop_value.length());        
    }
    delete command;
    free(work_handle);    
}
}