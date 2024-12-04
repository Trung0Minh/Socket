// Config.h
#pragma once
#include <string>

class Config {
private:
    static Config* instance;

    // Constructor là private để thực hiện Singleton pattern
    Config();

public:
    // Singleton instance getter
    static Config* getInstance();

    // Prevent copying
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // Các thông tin cấu hình
    const std::string TOKEN_FILE;
    const std::string CLIENT_ID;
    const std::string CLIENT_SECRET;
};