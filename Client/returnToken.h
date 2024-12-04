#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <curl/curl.h>
#include "json.hpp"
#include "Config.h"

using json = nlohmann::json;

class TokenManager {
private:
    json token_data;
    Config* config;

    // Callback function for CURL to write data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // Token validity checks
    bool isTokenExpired();
    bool refreshToken();

    // Token loading and utility functions
    bool loadTokenFile();

public:
    TokenManager() : config(Config::getInstance()) {}
    std::string getValidAccessToken();
};