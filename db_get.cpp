#include <stdlib.h>
#include <string.h>
#include <memory>

//Mysql include
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "db_get.h"
#include "client.h"
#include "db_proxy_server.h"

namespace dbproxy
{

void DBGet::Process(DBGetCommand *command){
    printf("get from redis\n");
    redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(command->player_id);
    redisAsyncCommand(redis_context,DBGet::GetRedisCallback,command,
        "hget %s %s",command->player_id.c_str(),command->prop_name.c_str());
}

void DBGet::GetRedisCallback(redisAsyncContext *context, void *reply, void *privdata){
    redisReply *redis_replay = (redisReply*)reply;
    DBGetCommand* command = (DBGetCommand*)privdata;
    if (redis_replay == NULL || command == NULL){
        return;
    }
    
    if(redis_replay->type == REDIS_REPLY_STRING){
        command->client->Response(std::string(redis_replay->str,redis_replay->len));
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
    DBGetCommand* command = (DBGetCommand*) work_handle->data;
    if(command == NULL){
        free(work_handle);
        return;
    }

    if(command->query_result == DBGetCommand::FOUNDED){
        redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(command->player_id);
        redisAsyncCommand(redis_context,DBGet::SetRedisCallback,command,
            "hset %s %s %b",
            command->player_id.c_str(),
            command->prop_name.c_str(),
            command->prop_value.c_str(),
            command->prop_value.length());
        command->client->Response(command->prop_value);
    }else if(command->query_result == DBGetCommand::NOT_FOUNDED){
        command->client->Response("not found in db");
    }else{
        command->client->Response("query db failed");
    }
    free(work_handle);
}
}