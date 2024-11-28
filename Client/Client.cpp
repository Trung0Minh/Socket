// Client.cpp
#include "Client.h"
#include <iostream>
#include <sstream>
#include <fstream>

Client::Client() :
    clientSocket(std::make_unique<ClientSocket>()),
    emailMonitor(std::make_unique<EmailMonitor>(this)),
    running(false),
    shouldCheckConnection(false),
    reconnectionAttempts(0),
    logCallback(nullptr),
    lastSenderEmail("") {

    // Set up email monitor callback
    emailMonitor->setCallback([this](const std::string& from, const std::string& subject, const std::string& body) {
        lastSenderEmail = from;
        std::string command;

        // Read COMMAND from email
        size_t cmdStart = body.find("COMMAND=");
        if (cmdStart != std::string::npos) {
            cmdStart += 8;
            size_t cmdEnd = body.find("\n", cmdStart);
            if (cmdEnd != std::string::npos) {
                command = body.substr(cmdStart, cmdEnd - cmdStart);
                command.erase(0, command.find_first_not_of(" \t\r\n"));
                command.erase(command.find_last_not_of(" \t\r\n") + 1);
            }
        }

        if (!command.empty()) {
            log("Executing command: " + command);
            std::string response;
            bool commandSuccess = false;

            if (sendData(command)) {
                if (receiveData(response)) {
                    log("Command executed successfully");
                    log("Response: " + response);
                    commandSuccess = true;
                }
                else {
                    log("Failed to receive response from server");
                    response = "Failed to receive response from server";
                }
            }
            else {
                log("Failed to send command to server");
                response = "Failed to send command to server";
            }

            // Gửi email response sau khi đã xử lý command xong
            if (commandSuccess) {
                sendEmail(lastSenderEmail, "Command Response", response);
            }
            else {
                sendEmail(lastSenderEmail, "Command Execution Error", response);
            }
        }
        else {
            log("Invalid email format: Missing COMMAND");
            sendEmail(lastSenderEmail, "Invalid Request", "Email must contain COMMAND");
        }
        });
}

Client::~Client() {
    stop();
}

void Client::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

void Client::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    }
}

void Client::setEmailCallback(EmailMonitor::EmailCallback cb) {
    if (emailMonitor) {
        emailMonitor->setCallback(cb);
    }
}

void Client::startEmailMonitor() {
    if (emailMonitor) {
        emailMonitor->start();
    }
}

void Client::stopEmailMonitor() {
    if (emailMonitor) {
        emailMonitor->stop();
        log("Email monitor stopped");
    }
}

bool Client::isEmailMonitorRunning() const {
    return emailMonitor && emailMonitor->isRunning();
}

//bool Client::initializeConnection(const std::string& serverIP, const std::string& port) {
//    std::lock_guard<std::mutex> lock(socketMutex);
//    if (!clientSocket->Connect(serverIP.c_str(), port.c_str(), AF_INET)) {
//        //log("Failed to connect to server at " + serverIP + ":" + port);
//        return false;
//    }
//    //log("Successfully connected to " + serverIP + ":" + port);
//    currentServerIP = serverIP;
//    currentPort = port;
//    return true;
//}

bool Client::connect(const std::string& serverIP, const std::string& port) {
    if (isConnected() && currentServerIP == serverIP && currentPort == port) {
        log("Already connected to " + serverIP + ":" + port);
        return true;
    }

    if (isConnected()) {
        disconnect();
    }

    std::lock_guard<std::mutex> lock(socketMutex);
    if (!clientSocket->Connect(serverIP.c_str(), port.c_str(), AF_INET)) {
        log("Failed to connect to server at " + serverIP + ":" + port);
        return false;
    }

    log("Successfully connected to " + serverIP + ":" + port);
    currentServerIP = serverIP;
    currentPort = port;
    reconnectionAttempts = 0;  // Reset reconnection attempts
    shouldCheckConnection = true;  // Enable connection checking
    return true;
}

void Client::disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (clientSocket) {
        clientSocket->Close();
        currentServerIP.clear();
        currentPort.clear();
        reconnectionAttempts = 0;
        log("Disconnected from server");
    }
}

bool Client::isConnected() const {
    return clientSocket && clientSocket->IsConnected();
}

// Sửa lại hàm sendData
bool Client::sendData(const std::string& data) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!clientSocket->IsConnected()) {
        log("Cannot send data: Not connected");
        return false;
    }

    if (!clientSocket->Send(data.c_str(), data.length())) {
        log("Failed to send data");
        return false;
    }
    return true;
}

// Sửa lại hàm receiveData trong Client
bool Client::receiveData(std::string& response) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!clientSocket->IsConnected()) {
        log("Cannot receive data: Not connected");
        return false;
    }

    char buffer[DEFAULT_BUFLEN];
    int bytesReceived = clientSocket->Receive(buffer, DEFAULT_BUFLEN);

    if (bytesReceived > 0) {
        response.assign(buffer, bytesReceived);
        return true;
    }
    else if (bytesReceived == 0) {
        log("No data received (timeout)");
        return false;
    }
    else {
        log("Connection error while receiving data");
        disconnect();
        return false;
    }
}

bool Client::executeCommand(const std::string& serverIP, const std::string& command, std::string& response) {
    if (!clientSocket->IsConnected()) {
        log("Lost connection to server, attempting to reconnect...");
        if (!connect(serverIP)) {
            log("Reconnection failed");
            return false;
        }
    }

    if (!sendData(command)) {
        log("Failed to send command - Connection might be lost");
        return false;
    }

    if (!receiveData(response)) {
        log("Failed to receive response - Connection might be lost");
        return false;
    }

    return true;
}

bool Client::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent,
    const std::vector<uint8_t>& attachment,
    const std::string& attachmentName) {

    log("Sending email to: " + to);
    bool success = emailSender.sendEmail(to, subject, textContent, attachment, attachmentName);

    if (success) {
        log("Email sent successfully");
    }
    else {
        log("Failed to send email");
    }

    return success;
}

bool Client::start(const std::string& serverIP) {
    if (!isConnected()) {
        if (!connect(serverIP, "54000")) {
            log("Failed to connect during startup");
            return false;
        }
    }

    running = true;
    shouldCheckConnection = true;

    // Khởi động thread kiểm tra kết nối
    connectionCheckerThread = std::thread(&Client::connectionCheckerLoop, this);

    startEmailMonitor();
    log("Client started successfully");
    return true;
}

void Client::stop() {
    if (running) {
        running = false;
        shouldCheckConnection = false;

        // Đợi thread kiểm tra kết nối kết thúc
        if (connectionCheckerThread.joinable()) {
            connectionCheckerThread.join();
        }

        stopEmailMonitor();
        disconnect();
        log("Client stopped successfully");
    }
}

void Client::connectionCheckerLoop() {
    while (shouldCheckConnection && running) {
        if (!clientSocket->IsConnected()) {
            log("Connection lost to server at " + currentServerIP + ":" + currentPort);

            // Thử kết nối lại
            if (reconnectionAttempts < MAX_RECONNECTION_ATTEMPTS) {
                std::lock_guard<std::mutex> lock(socketMutex);
                reconnectionAttempts++;

                log("Reconnection attempt " + std::to_string(reconnectionAttempts) + " of " + std::to_string(MAX_RECONNECTION_ATTEMPTS));

                if (!clientSocket->Connect(currentServerIP.c_str(), currentPort.c_str(), AF_INET)) {
                    log("Reconnection attempt failed");
                }
                else {
                    log("Successfully reconnected to server");
                    reconnectionAttempts = 0; // Reset counter on successful reconnection
                }
            }
            else {
                log("Maximum reconnection attempts reached. Connection to server " + currentServerIP + " is permanently lost");
                log("Please reconnect manually with new server IP");

                // Reset connection-related variables
                currentServerIP.clear();
                currentPort.clear();
                reconnectionAttempts = 0;

                // Stop the connection checker
                shouldCheckConnection = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}