#pragma once

class Config {
public:
    Config();
    void parse_arg(int argc, char* argv[]);

    int PORT;
};

