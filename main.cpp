#include "signal.h"

#include "db_proxy_server.h"


int main(int argc, char* argv[]){
    signal(SIGPIPE, SIG_IGN);
    dbproxy::DBProxyServer::Instance()->Start(1080);
    return 0;
}