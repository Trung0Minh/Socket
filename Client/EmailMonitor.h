#pragma once
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include "json.hpp"
#include "returnToken.h"
#include "Config.h"

class EmailMonitor {
public:
    using EmailCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;
    using CommandExecutor = std::function<bool(const std::string&, const std::string&, std::string&, const std::string&)>;

    EmailMonitor();
    ~EmailMonitor();

    void setCallback(EmailCallback cb) { callback = std::move(cb); }
    void setLogCallback(std::function<void(const std::string&)> cb) { logCallback = std::move(cb); }
    void setCommandExecutor(CommandExecutor executor) { commandExecutor = std::move(executor); }
    void start();
    void stop();
    bool isRunning() const { return running; }

private:
    struct EmailContent {
        std::string from;
        std::string subject;
        std::string body;
        std::string serverIp;
        std::string command;
    };

    struct CurlResponse {
        std::string data;
        long httpCode;
        bool success;
    };

    bool running;
    std::thread monitorThread;
    std::queue<std::string> emailQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    TokenManager tokenManager;
    EmailCallback callback;
    std::function<void(const std::string&)> logCallback;
    CommandExecutor commandExecutor;

    static std::string trim(const std::string& str);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    void log(const std::string& message);
    void monitorEmails();
    bool checkNewEmails();
    std::string getEmailContent(const std::string& emailId);
    bool markEmailAsRead(const std::string& emailId);
    EmailContent parseEmailContent(const nlohmann::json& emailData);
    void processEmail(const std::string& emailId);
    CurlResponse performCurlRequest(const std::string& url, const std::string& accessToken,
        const std::string& method = "GET",
        const std::string& postData = "");
};