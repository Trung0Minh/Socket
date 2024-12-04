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
    // Logging callback function
    std::function<void(const std::string&)> logCallback;
    std::function<void(const std::string&)> reconnectCallback;

    // Network related members
    std::unique_ptr<ClientSocket> clientSocket;
    std::string currentServerIP;
    std::string currentPort;
    std::mutex socketMutex;
    static const int DEFAULT_BUFLEN = 4096;

    // Connection checking related members
    std::thread connectionCheckerThread;
    bool shouldCheckConnection;
    static const int MAX_RECONNECTION_ATTEMPTS = 3;
    int reconnectionAttempts;
    bool needReconnection;

    // Email related members
    EmailSender emailSender;
    std::unique_ptr<EmailMonitor> emailMonitor;
    std::string lastSenderEmail;

    // Control related members
    bool running;

    // Helper function for logging
    void log(const std::string& message);

    // Connection checking loop function
    void connectionCheckerLoop();

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
        const std::string& textContent,
        const std::vector<uint8_t>& attachment = std::vector<uint8_t>(),
        const std::string& attachmentName = "");

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

    // Callback for reconnection events
    void setReconnectCallback(std::function<void(const std::string&)> callback);
};