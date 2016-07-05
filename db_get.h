#ifndef __DBPROXY_DB_GET_H__
#define __DBPROXY_DB_GET_H__
#include <string>

#include <libuv/uv.h>
#include <hiredis/async.h>

namespace dbproxy
{
class Client;
class Message;
struct DBGetCommand
{
    enum MYSQL_QUERY_RET{
        INIT = 0,
        FOUNDED = 1,
        NOT_FOUNDED = 2,
        QUERY_FAILED = 3
    };

    Client* client;
    uint32_t sn;
    std::string player_id;
    std::string prop_name;
    MYSQL_QUERY_RET query_result;
    std::string prop_value;
    DBGetCommand():client(NULL),sn(0),player_id(""),prop_name(""),prop_value(""),query_result(INIT){};
};

class DBGet{
public:
    static void Process(Message *message,Client* client);
    static void GetRedisCallback(redisAsyncContext *c, void *reply, void *privdata);
    static void SetRedisCallback(redisAsyncContext *c, void *reply, void *privdata);
    static void QueryMysql(uv_work_t *work_handle);
    static void QueryMysqlCallback(uv_work_t *work_handle, int status);
};

}

#endif//__DBPROXY_DB_GET_H__