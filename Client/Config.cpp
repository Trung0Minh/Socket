// config.cpp
#include "config.h"

Config* Config::instance = nullptr;

Config::Config() :
    TOKEN_FILE("YOUR TOKEN FILE"),
    CLIENT_ID("YOUR CLIENT ID"),
    CLIENT_SECRET("YOUR CLIENT SECRET")
{
}

Config* Config::getInstance() {
    if (instance == nullptr) {
        instance = new Config();
    }
    return instance;
}
