// returnToken.h
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <curl/curl.h>
#include "json.hpp"
#include "config.h"

using json = nlohmann::json;

class TokenManager {
private:
    json token_data;
    Config* config;

    bool isTokenExpired();
    bool refreshToken();

public:
    TokenManager() : config(Config::getInstance()) {}
    // Callback function for CURL to write data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    bool loadTokenFile();
    std::string getValidAccessToken();
    void printTokenData();
};



