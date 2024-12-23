#pragma once
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include "EmailSender.h"
#include "ClientSocket.h" 
#include "returnToken.h"
#include "EmailMonitor.h"

class Client {
private:
    std::function<void(const std::string&)> logCallback;
    std::unique_ptr<ClientSocket> clientSocket;
    std::string currentServerIP;
    std::string currentPort;
    std::mutex socketMutex;

    EmailSender emailSender;
    std::unique_ptr<EmailMonitor> emailMonitor;
    std::string lastSenderEmail;

    bool running = false;
    static bool shouldMarkAsReadDefault;

    static constexpr int CHECK_INTERVAL = 1;
    static constexpr int DEFAULT_BUFLEN = 1024 * 1024 * 25;
    static constexpr const char* DEFAULT_PORT = "54000";

    void log(const std::string& message);
    void initializeEmailMonitor();
    void handleConnectionError(const std::string& errorMsg, const std::string& senderEmail = "");

public:
    Client();
    ~Client();

    void setLogCallback(std::function<void(const std::string&)> callback);
    bool connect(const std::string& serverIP, const std::string& port = DEFAULT_PORT);
    void disconnect();
    bool isConnected() const;
    bool sendData(const std::string& data);
    bool receiveData(std::string& response);

    bool executeCommand(const std::string& serverIP,
        const std::string& command,
        std::string& response,
        const std::string& senderEmail = "",
        bool& shouldMarkAsRead = shouldMarkAsReadDefault);

    bool sendEmail(const std::string& to,
        const std::string& subject,
        const std::string& textContent);

    void setEmailCallback(EmailMonitor::EmailCallback cb);
    void startEmailMonitor();
    void stopEmailMonitor();
    bool isEmailMonitorRunning() const;

    bool start(const std::string& serverIP);
    void stop();
    bool isRunning() const;
};