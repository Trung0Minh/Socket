// EmailMonitor.h
#pragma once
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "json.hpp"
#include "returnToken.h"

class Client;

class EmailMonitor {
public:
    using EmailCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;

private:
    struct EmailContent {
        std::string from;
        std::string subject;
        std::string body;
        std::string serverIp;
        std::string command;
    };

    Client* client;
    bool running;
    std::thread monitorThread;
    std::queue<std::string> emailQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    TokenManager tokenManager;
    EmailCallback callback;

    static std::string trim(const std::string& str);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    void monitorEmails();
    bool checkNewEmails();
    std::string getEmailContent(const std::string& emailId);
    bool markEmailAsRead(const std::string& emailId);
    EmailContent parseEmailContent(const nlohmann::json& emailData);
    void processEmail(const std::string& emailId);
    bool executeCommand(const EmailContent& email);

public:
    EmailMonitor(Client* clientPtr);
    ~EmailMonitor();

    void setCallback(EmailCallback cb) { callback = std::move(cb); }
    void start();
    void stop();
    bool isRunning() const { return running; }
};