#include <stdlib.h>
#include <string.h>

#include "db_get.h"
#include "client.h"
#include "db_proxy_server.h"

namespace dbproxy
{

void DBGet::Process(DBGetCommand *command){
    redisAsyncContext* redis_context = DBProxyServer::Instance()->GetRedisContext(command->player_id);
    fprintf(stdout, "execute redis command %s\n", command->player_id.c_str());
    redisAsyncCommand(redis_context,DBGet::RedisCommandCallback,command,"GET hhb_fix",command->player_id.c_str(),command->key.c_str());
}

void DBGet::RedisCommandCallback(redisAsyncContext *context, void *reply, void *privdata){
    redisReply *redis_replay = (redisReply*)reply;
    DBGetCommand* command = (DBGetCommand*)privdata;
    if (redis_replay == NULL){
        return;
    }
    fprintf(stdout,"redis command return, %s\n",redis_replay->str);
    uint16_t replyLen = redis_replay->len + 3;
    uv_buf_t write_buffer;
    command->client->AllocWriteBuffer(replyLen,&write_buffer);
    *(uint16_t*)(write_buffer.base) = htons(replyLen);
    *(uint8_t*)(write_buffer.base+2) = 0;
    if(redis_replay->len > 0){
        memcpy(write_buffer.base+3,redis_replay->str,redis_replay->len);
    }    
    uv_write_t *req = (uv_write_t *) malloc(sizeof(uv_write_t));    
    uv_write(req,command->client->GetHandle(),&write_buffer,1,Client::WriteCallback);
}

}