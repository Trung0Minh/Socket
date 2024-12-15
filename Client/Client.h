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
    // Logging callback function
    std::function<void(const std::string&)> logCallback;

    // Network related members
    std::unique_ptr<ClientSocket> clientSocket;
    std::string currentServerIP;
    std::string currentPort;
    std::mutex socketMutex;

    // Email related members
    EmailSender emailSender;
    std::unique_ptr<EmailMonitor> emailMonitor;
    std::string lastSenderEmail;

    // Control related members
    bool running;
    std::atomic<bool> connectionCheckRunning;
    std::thread connectionCheckThread;
    std::atomic<bool> connectionLostLogged;

    const int CHECK_INTERVAL = 1;
    const int DEFAULT_BUFLEN = 1024 * 1024 * 10;
    const std::string DEFAULT_PORT = "54000";

    // Helper function for logging
    void log(const std::string& message);

    // Helper function for connection checking
    void checkConnection();

public:
    // Constructor and Destructor
    Client();
    ~Client();

    // Logging functions
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
        const std::string& textContent);

    // Command processing
    bool executeCommand(const std::string& serverIP,
        const std::string& command,
        std::string& response,
        const std::string& senderEmail = "");

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