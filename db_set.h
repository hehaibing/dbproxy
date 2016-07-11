#ifndef __DBPROXY_DB_SET_H
#define __DBPROXY_DB_SET_H

#include <string>
#include <libuv/uv.h>
#include <hiredis/async.h>

namespace dbproxy {

class Client;
class Message;

struct DBSetCommand
{
    enum MYSQL_INSERT_RET{
        INIT = 0,
        FAILED = 1,
        SUCCESS = 2
    };
    Client* client;
    uint32_t sn;
    std::string player_id;
    std::string prop_name;
    std::string prop_value;
    MYSQL_INSERT_RET insert_result;
};

class DBSet{
public:
    static void Process(Message *message,Client* client);
    static void SetRedisCallback(redisAsyncContext *c, void *reply, void *privdata);
    static void InsertMysql(uv_work_t *work_handle);
    static void InsertMysqlCallback(uv_work_t *work_handle, int status);
};
}
#endif //__DBPROXY_DB_SET_H