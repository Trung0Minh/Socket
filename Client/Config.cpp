// Config.cpp
#include "Config.h"

Config* Config::instance = nullptr;

Config::Config() :
    TOKEN_FILE("token.json"),
    CLIENT_ID("235219765051-jgrv7igsp4ki9r8ossjig9hpfk1ko3dt.apps.googleusercontent.com"),
    CLIENT_SECRET("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")
{
}

Config* Config::getInstance() {
    if (instance == nullptr) {
        instance = new Config();
    }
    return instance;
}

std::string Config::getTokenFile() const {
    return TOKEN_FILE;
}