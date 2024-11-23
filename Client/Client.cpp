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

        // Lưu địa chỉ email người gửi
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
            log("Executing command: " + command);

            // Gửi command tới server
            if (sendData(command)) {
                std::string response;
                // Nhận kết quả từ server
                if (receiveData(response)) {
                    log("Command executed successfully");
                    log("Response: " + response);
                    // receiveData đã tự động gửi kết quả qua email cho user
                }
                else {
                    log("Failed to receive response from server");
                    // Gửi thông báo lỗi cho user
                    sendEmail(lastSenderEmail, "Command Execution Error", "Failed to receive response from server");
                }
            }
            else {
                log("Failed to send command to server");
                // Gửi thông báo lỗi cho user
                sendEmail(lastSenderEmail, "Command Execution Error", "Failed to send command to server");
            }
        }
        else {
            log("Invalid email format: Missing COMMAND");
            // Gửi thông báo lỗi cho user
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
        log("Failed to connect to server at " + serverIP + ":" + port);
        return false;
    }
    log("Successfully connected to " + serverIP + ":" + port);
    currentServerIP = serverIP;
    currentPort = port;
    return true;
}

bool Client::connect(const std::string& serverIP, const std::string& port) {
    if (isConnected()) {
        disconnect();
    }
    return initializeConnection(serverIP, "54000"); // Sử dụng port 54000 để match với server
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

        // Gửi kết quả qua email
        if (!lastSenderEmail.empty()) {
            std::string emailSubject = "Command Execution Result";
            std::string emailBody = "Command execution completed.\n\nResult:\n" + response;

            if (sendEmail(lastSenderEmail, emailSubject, emailBody)) {
                log("Result sent to " + lastSenderEmail + " via email");
            }
            else {
                log("Failed to send result via email to " + lastSenderEmail);
            }
        }

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

    if (handleEmailMonitorCommand(command, response)) {
        log("Email monitor command executed: " + response);
        return true;
    }

    if (!isConnected() && !connect(serverIP)) {
        response = "Failed to connect to server";
        log(response);
        return false;
    }

    if (handleNetworkCommand(command, response)) {
        log("Network command executed: " + response);
        return true;
    }

    if (handleEmailCommand(command, response)) {
        log("Email command executed: " + response);
        return true;
    }

    if (sendData(command)) {
        if (receiveData(response)) {
            log("Command executed successfully. Response: " + response);
            return true;
        }
    }

    response = "Failed to execute command";
    log(response);
    return false;
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

void Client::processEmailCommand(const std::string& command,
    const std::string& to,
    const std::string& subject,
    const std::string& content,
    const std::string& filePath) {

    if (command == "email_send_text") {
        sendEmail(to, subject, content);
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
    // Kết nối tới server trước
    //if (!connect(serverIP)) {
    //    log("Failed to connect to server at " + serverIP);
    //    return false;
    //}
    log("Successfully connected to server at " + serverIP);

    // Sau đó mới bắt đầu EmailMonitor
    running = true;
    emailMonitor->start();
    log("Email monitoring started");
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