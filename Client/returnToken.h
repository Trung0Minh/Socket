// returnToken.h
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

class TokenManager {
private:
    static const std::string TOKEN_FILE;
    static const std::string CLIENT_ID;
    static const std::string CLIENT_SECRET;
    json token_data;

    bool isTokenExpired();
    bool refreshToken();

public:
    // Callback function for CURL to write data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    bool loadTokenFile();
    std::string getValidAccessToken();
    void printTokenData();
};



