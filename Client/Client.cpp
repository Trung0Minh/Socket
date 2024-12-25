#include "Client.h"
#include <vector>

bool Client::shouldMarkAsReadDefault = false;

void Client::log(const std::string& message) {
    if (logCallback) logCallback(message);
}

void Client::initializeEmailMonitor() {
    emailMonitor->setCommandExecutor(
        [this](const std::string& serverIP, const std::string& command,
            std::string& response, const std::string& senderEmail, bool& shouldMarkAsRead) {
                try {
                    if (serverIP.empty() || command.empty()) {
                        log("Error: Invalid command parameters");
                        return false;
                    }
                    lastSenderEmail = senderEmail;
                    return executeCommand(serverIP, command, response, senderEmail, shouldMarkAsRead);
                }
                catch (const std::exception& e) {
                    log("Error in command executor: " + std::string(e.what()));
                    return false;
                }
        }
    );
}

void Client::handleConnectionError(const std::string& errorMsg, const std::string& senderEmail) {
    log(errorMsg);
    if (!senderEmail.empty()) {
        std::string formattedMessage = "TYPE:text|SIZE:" + std::to_string(errorMsg.size()) + "\n" + errorMsg;

        if (!sendEmail(senderEmail, "Command Execution Failed", formattedMessage)) {
            log("Failed to send error notification email. Email content: " + formattedMessage);
        }
    }
}

Client::Client() :
    clientSocket(std::make_unique<ClientSocket>()),
    emailMonitor(std::make_unique<EmailMonitor>()) {
    initializeEmailMonitor();
}

Client::~Client() {
    stop();
}

void Client::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
    emailMonitor->setLogCallback([this](const std::string& msg) { log(msg); });
}

bool Client::connect(const std::string& serverIP, const std::string& port) {
    if (isConnected() && currentServerIP == serverIP && currentPort == port) {
        log("Already connected to " + serverIP + ":" + port);
        return true;
    }

    disconnect();
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

void Client::disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (clientSocket) {
        clientSocket->Close();
        clientSocket = std::make_unique<ClientSocket>();
        stopEmailMonitor();
    }
    currentServerIP.clear();
    currentPort.clear();
}

bool Client::isConnected() const {
    return clientSocket && clientSocket->IsConnected();
}

bool Client::sendData(const std::string& data) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!isConnected()) {
        log("Cannot send data: Not connected");
        return false;
    }
    if (!clientSocket->Send(data.c_str(), data.length())) {
        log("Failed to send data");
        disconnect();
        return false;
    }
    return true;
}

bool Client::receiveData(std::string& response) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!isConnected()) {
        log("Cannot receive data: Not connected");
        return false;
    }

    std::vector<char> buffer(DEFAULT_BUFLEN);
    // Clear buffer
    std::fill(buffer.begin(), buffer.end(), 0);

    size_t bytesReceived = clientSocket->Receive(buffer.data(), DEFAULT_BUFLEN);

    if (bytesReceived > 0) {
        response.assign(buffer.data(), bytesReceived);
        return true;
    }

    log(bytesReceived == 0 ? "No data received (timeout)" : "Connection error while receiving data");
    disconnect();
    return false;
}

bool Client::executeCommand(const std::string& serverIP, const std::string& command,
    std::string& response, const std::string& senderEmail, bool& shouldMarkAsRead) {
    try {
        shouldMarkAsRead = false; // Mặc định là false

        if (isConnected() && currentServerIP != serverIP) {
            handleConnectionError("Error: Command intended for server " + serverIP +
                " but currently connected to " + currentServerIP, senderEmail);
            shouldMarkAsRead = true; // Đánh dấu là đã đọc khi có lỗi IP
            return false;
        }

        if (!isConnected() && !connect(serverIP)) {
            handleConnectionError("Failed to connect to server " + serverIP, senderEmail);
            shouldMarkAsRead = true; // Đánh dấu là đã đọc khi không thể kết nối
            return false;
        }

        if (!sendData(command) || !receiveData(response)) {
            shouldMarkAsRead = true; // Đánh dấu là đã đọc khi có lỗi gửi/nhận
            return false;
        }

        if (!senderEmail.empty() && !response.empty()) {
            if (!sendEmail(senderEmail, "Command Execution Result", response)) {
                log("Warning: Failed to send response email");
            }
        }
        shouldMarkAsRead = true; // Đánh dấu là đã đọc khi thành công
        return true;
    }
    catch (const std::exception& e) {
        log("Error executing command: " + std::string(e.what()));
        shouldMarkAsRead = true; // Đánh dấu là đã đọc khi có exception
        return false;
    }
}

bool Client::sendEmail(const std::string& to, const std::string& subject,
    const std::string& textContent) {
    log("Sending email to: " + to);
    bool success = emailSender.sendEmail(to, subject, textContent);
    log(success ? "Email sent successfully" : "Failed to send email");
    return success;
}

void Client::setEmailCallback(EmailMonitor::EmailCallback cb) {
    emailMonitor->setCallback(cb);
}

void Client::startEmailMonitor() {
    emailMonitor->start();
}

void Client::stopEmailMonitor() {
    emailMonitor->stop();
}

bool Client::isEmailMonitorRunning() const {
    return emailMonitor->isRunning();
}

bool Client::start(const std::string& serverIP) {
    if (!isConnected() && !connect(serverIP)) {
        log("Failed to connect during startup");
        return false;
    }

    running = true;
    emailMonitor->setLogCallback(logCallback);
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

bool Client::isRunning() const {
    return running;
}