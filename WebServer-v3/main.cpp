#include "config.h"
#include "webserver.h"


int main(int argc, char* argv[]) {
    Config cfg;
    cfg.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(cfg.PORT);

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();
    return 0;
}

