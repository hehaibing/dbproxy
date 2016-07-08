#include "signal.h"
#include "db_proxy_server.h"
#include <google/protobuf/stubs/common.h>

void sig_handler( int sig)
{
   if(sig == SIGINT){
        dbproxy::DBProxyServer::Instance()->Stop();
   }
}

void do_exit_clean(){
    google::protobuf::ShutdownProtobufLibrary();
}
int main(int argc, char* argv[]){
    signal(SIGPIPE, SIG_IGN);
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    atexit(do_exit_clean);
    dbproxy::DBProxyServer::Instance()->Start(1080);
    return 0;
}