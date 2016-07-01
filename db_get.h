#ifndef __DBPROXY_DB_GET_H__
#define __DBPROXY_DB_GET_H__
#include <string>

#include <libuv/uv.h>
#include <hiredis/async.h>

namespace dbproxy
{
class Client;
struct DBGetCommand
{
	Client* client;
	std::string player_id;
	std::string key;
};

class DBGet{
public:
	static void Process(DBGetCommand* context);
	static void RedisCommandCallback(redisAsyncContext *c, void *reply, void *privdata);
	static void WriteClientFinished(uv_write_t *req, int status);	
};

}

#endif//__DBPROXY_DB_GET_H__