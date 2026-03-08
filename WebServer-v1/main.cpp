#include "config.h"
#include "webserver.h"

int main(int argc, char* argv[]) {
    Config cfg;
    cfg.parse_arg(argc, argv);

    WebServer server;
    server.init(cfg.PORT);
    server.run_loop();
    return 0;
}

