// Client.h
#pragma once
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include "EmailSender.h"
#include "ClientSocket.h" 
#include "returnToken.h"
#include "EmailMonitor.h"

class Client {
private:
    std::unique_ptr<ClientSocket> clientSocket;
    TokenManager tokenManager;
    EmailSender emailSender;
    std::unique_ptr<EmailMonitor> emailMonitor;
    bool running;
    std::mutex socketMutex;
    std::function<void(const std::string&)> logCallback;

    static const int DEFAULT_BUFLEN = 4096;
    std::string currentServerIP;
    std::string currentPort;

    // Helper methods
    //bool initializeConnection(const std::string& serverIP, const std::string& port);

    // Logging helper
    void log(const std::string& message);

    std::string lastSenderEmail;  // Thêm biến này để lưu email người gửi gần nhất

public:
    Client();
    ~Client();

    // Logging
    void setLogCallback(std::function<void(const std::string&)> callback);

    // Network operations
    bool connect(const std::string& serverIP, const std::string& port = "54000");
    void disconnect();
    bool isConnected() const;
    bool sendData(const std::string& data);
    bool receiveData(std::string& response);


    // Email operations
    bool sendEmail(const std::string& to,
        const std::string& subject,
        const std::string& textContent,
        const std::vector<uint8_t>& attachment = std::vector<uint8_t>(),
        const std::string& attachmentName = "");

    // Command processing
    bool executeCommand(const std::string& serverIP, const std::string& command, std::string& response);

    // Email monitoring operations
    void setEmailCallback(EmailMonitor::EmailCallback cb);
    void startEmailMonitor();
    void stopEmailMonitor();
    bool isEmailMonitorRunning() const;

    // Control operations
    bool start(const std::string& serverIP);
    void stop();
    bool isRunning() const { return running; }
};
