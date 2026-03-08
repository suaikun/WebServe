#include "config.h"

#include <cstdlib>
#include <getopt.h>

Config::Config() : PORT(9006) {}

void Config::parse_arg(int argc, char* argv[]) {
    int opt;
    const char* str = "p:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
        case 'p':
            PORT = std::atoi(optarg);
            break;
        default:
            break;
        }
    }
}

