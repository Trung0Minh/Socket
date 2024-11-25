// Client.cpp
#include "Client.h"
#include <iostream>
#include <sstream>
#include <fstream>

Client::Client() :
    clientSocket(std::make_unique<ClientSocket>()),
    emailMonitor(std::make_unique<EmailMonitor>(this)),
    running(false),
    logCallback(nullptr),
    lastSenderEmail("") {
    // Set up email monitor callback
    emailMonitor->setCallback([this](const std::string& from, const std::string& subject, const std::string& body) {
        log("\n=== New Request Email ===");
        log("From: " + from);
        log("Subject: " + subject);
        log("Content:\n" + body);
        log("=====================");

        lastSenderEmail = from;
        std::string command;

        // Đọc COMMAND từ email
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

        // Kiểm tra và thực thi yêu cầu
        if (!command.empty()) {
            std::string response;
            if (sendData(command)) {
                if (receiveData(response)) {
                    // Chỉ gửi email kết quả, không log lại quá trình thực thi
                    sendEmail(lastSenderEmail, "Command Execution Result",
                        "Command execution completed.\n\nResult:\n" + response,
                        std::vector<uint8_t>(), "", false); // thêm tham số false để không log
                }
                else {
                    sendEmail(lastSenderEmail, "Command Execution Error",
                        "Failed to receive response from server",
                        std::vector<uint8_t>(), "", false);
                }
            }
            else {
                sendEmail(lastSenderEmail, "Command Execution Error",
                    "Failed to send command to server",
                    std::vector<uint8_t>(), "", false);
            }
        }
        else {
            sendEmail(lastSenderEmail, "Invalid Request",
                "Email must contain COMMAND",
                std::vector<uint8_t>(), "", false);
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
        log("Email monitor started");
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

bool Client::initializeConnection(const std::string& serverIP, const std::string& port) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!clientSocket->Connect(serverIP.c_str(), port.c_str(), AF_INET)) {
        //log("Failed to connect to server at " + serverIP + ":" + port);
        return false;
    }
    //log("Successfully connected to " + serverIP + ":" + port);
    currentServerIP = serverIP;
    currentPort = port;
    return true;
}

bool Client::connect(const std::string& serverIP, const std::string& port) {
    // Nếu đã kết nối đến cùng một địa chỉ, không cần kết nối lại
    if (isConnected() && currentServerIP == serverIP && currentPort == port) {
        log("Already connected to " + serverIP + ":" + port);
        return true;
    }

    // Nếu đang kết nối đến địa chỉ khác, ngắt kết nối cũ
    if (isConnected()) {
        disconnect();
    }

    return initializeConnection(serverIP, port);
}

void Client::disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (clientSocket) {
        clientSocket->Close();
        log("Disconnected from server");
    }
}

bool Client::isConnected() const {
    return clientSocket && clientSocket->IsConnected();
}

bool Client::sendData(const std::string& data) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!isConnected()) {
        log("Cannot send data: Not connected to server");
        return false;
    }
    bool success = clientSocket->Send(data.c_str(), static_cast<int>(data.length()));
    if (success) {
        log("Data sent successfully");
    }
    else {
        log("Failed to send data");
    }
    return success;
}

bool Client::receiveData(std::string& response) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!isConnected()) {
        log("Cannot receive data: Not connected to server");
        return false;
    }

    char buffer[DEFAULT_BUFLEN];
    int bytesReceived = clientSocket->Receive(buffer, DEFAULT_BUFLEN);

    if (bytesReceived > 0) {
        response.assign(buffer, bytesReceived);
        log("Data received successfully");
        return true;
    }
    log("Failed to receive data");
    return false;
}

bool Client::handleNetworkCommand(const std::string& command, std::string& response) {
    if (command == "test_connection") {
        if (!isConnected()) {
            response = "Not connected to server";
            log(response);
            return false;
        }
        if (sendData("ping")) {
            if (receiveData(response)) {
                log("Connection test successful");
                return true;
            }
        }
        response = "Connection test failed";
        log(response);
        return false;
    }
    return false;
}

bool Client::handleEmailCommand(const std::string& command, std::string& response) {
    if (command.find("email_") == 0) {
        processEmailCommand(command);
        response = "Email command processed";
        log(response);
        return true;
    }
    return false;
}

bool Client::handleEmailMonitorCommand(const std::string& command, std::string& response) {
    if (command == "start_monitor") {
        startEmailMonitor();
        response = "Email monitor started";
        return true;
    }
    else if (command == "stop_monitor") {
        stopEmailMonitor();
        response = "Email monitor stopped";
        return true;
    }
    else if (command == "monitor_status") {
        response = isEmailMonitorRunning() ? "Monitor is running" : "Monitor is stopped";
        return true;
    }
    return false;
}

bool Client::executeCommand(const std::string& serverIP, const std::string& command, std::string& response) {
    log("Executing command: " + command);

    // Thử kết nối nếu chưa kết nối
    if (!isConnected()) {
        if (!connect(serverIP)) {
            response = "Failed to connect to server";
            log(response);
            return false;
        }
    }

    // Gửi lệnh
    if (!sendData(command)) {
        // Nếu gửi thất bại, thử kết nối lại một lần
        if (!connect(serverIP) || !sendData(command)) {
            response = "Failed to send command to server";
            log(response);
            return false;
        }
    }

    // Nhận phản hồi
    if (!receiveData(response)) {
        response = "Failed to receive response from server";
        log(response);
        return false;
    }

    log("Command executed successfully");
    return true;
}

bool Client::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent,
    const std::vector<uint8_t>& attachment,
    const std::string& attachmentName,
    bool shouldLog = true) {

    if (shouldLog) {
        log("Sending email to: " + to);
    }

    bool success = emailSender.sendEmail(to, subject, textContent, attachment, attachmentName);

    if (shouldLog) {
        if (success) {
            log("Email sent successfully");
        }
        else {
            log("Failed to send email");
        }
    }

    return success;
}

void Client::processEmailCommand(const std::string& command,
    const std::string& to,
    const std::string& subject,
    const std::string& content,
    const std::string& filePath) {

    if (command == "email_send_text") {
        //sendEmail(to, subject, content);
    }
    else if (command == "email_send_attachment") {
        if (!filePath.empty()) {
            std::ifstream file(filePath, std::ios::binary);
            if (file) {
                std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
                sendEmail(to, subject, content, fileData,
                    filePath.substr(filePath.find_last_of("/\\") + 1));
            }
            else {
                log("Failed to open attachment file: " + filePath);
            }
        }
    }
}

bool Client::start(const std::string& serverIP) {
    if (!isConnected()) {
        if (!connect(serverIP, "54000")) {
            log("Failed to connect during startup");
            return false;
        }
    }

    running = true;
    startEmailMonitor();
    log("Client started successfully");
    return true;
}

void Client::stop() {
    if (running) {
        running = false;
        stopEmailMonitor();
        disconnect();
        log("Client stopped successfully");
    }
}