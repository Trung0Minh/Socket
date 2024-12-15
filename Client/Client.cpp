#include "Client.h"
#include <iostream>
#include <sstream>
#include <fstream>

// Constructor initializes members and sets up email monitor callback
Client::Client() :
    clientSocket(std::make_unique<ClientSocket>()),
    emailMonitor(std::make_unique<EmailMonitor>()),
    emailSender(),
    running(false),
    connectionCheckRunning(false),
    connectionLostLogged(false),
    logCallback(nullptr) {

    emailMonitor->setCommandExecutor(
        [this](const std::string& serverIP, const std::string& command,
            std::string& response, const std::string& senderEmail) -> bool {
                try {
                    if (serverIP.empty() || command.empty()) {
                        log("Error: Invalid command parameters");
                        return false;
                    }

                    lastSenderEmail = senderEmail;

                    if (!executeCommand(serverIP, command, response, senderEmail)) {
                        log("Error: Command execution failed for server " + serverIP);
                        return false;
                    }

                    return true;
                }
                catch (const std::exception& e) {
                    log("Error in command executor: " + std::string(e.what()));
                    return false;
                }
        }
    );
}

Client::~Client() {
    stop();
}

void Client::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
    if (emailMonitor) {
        emailMonitor->setLogCallback(
            [this](const std::string& message) {
                if (logCallback) logCallback(message);
            }
        );
    }
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
    }

    currentServerIP.clear();
    currentPort.clear();
}

bool Client::isConnected() const {
    return clientSocket && clientSocket->IsConnected();
}

bool Client::sendData(const std::string& data) {
    std::lock_guard<std::mutex> lock(socketMutex);
    if (!clientSocket->IsConnected()) {
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
    if (!clientSocket->IsConnected()) {
        log("Cannot receive data: Not connected");
        return false;
    }

    std::vector<char> buffer(DEFAULT_BUFLEN);

    int bytesReceived = clientSocket->Receive(buffer.data(), DEFAULT_BUFLEN);

    if (bytesReceived > 0) {
        response.assign(buffer.data(), bytesReceived);
        return true;
    }
    else if (bytesReceived == 0) {
        log("No data received (timeout)");
        disconnect();
        return false;
    }
    else {
        log("Connection error while receiving data");
        disconnect();
        return false;
    }
}

bool Client::executeCommand(const std::string& serverIP,
    const std::string& command,
    std::string& response,
    const std::string& senderEmail) {

    try {
        // Kiểm tra nếu đang kết nối tới một server khác
        if (isConnected() && currentServerIP != serverIP) {
            std::string errorMsg = "Error: Command intended for server " + serverIP +
                " but currently connected to " + currentServerIP;
            log(errorMsg);
            response = errorMsg;
            return false;
        }

        // Nếu chưa kết nối hoặc kết nối đến đúng server
        if (!isConnected()) {
            if (!connect(serverIP)) {
                std::string errorMsg = "Failed to connect to server " + serverIP;
                log(errorMsg);
                response = errorMsg;

                if (!senderEmail.empty()) {
                    sendEmail(senderEmail, "Command Execution Failed", errorMsg);
                }
                return false;
            }
        }

        if (!sendData(command)) {
            log("Failed to send command");
            return false;
        }

        if (!receiveData(response)) {
            log("Failed to receive response");
            return false;
        }

        if (!senderEmail.empty() && !response.empty()) {
            if (!sendEmail(senderEmail, "Command Execution Result", response)) {
                log("Warning: Failed to send response email");
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        log("Error executing command: " + std::string(e.what()));
        return false;
    }
}

bool Client::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent) {

    log("Sending email to: " + to);
    bool success = emailSender.sendEmail(to, subject, textContent);

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
        if (!connect(serverIP, DEFAULT_PORT)) {
            log("Failed to connect during startup");
            return false;
        }
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
        connectionCheckRunning = false;
        if (connectionCheckThread.joinable()) {
            connectionCheckThread.join();
        }
        stopEmailMonitor();
        disconnect();
        log("Client stopped successfully");
    }
}

void Client::checkConnection() {
    while (connectionCheckRunning) {
        if (isConnected()) {
            // Optionally, you can send a keep-alive message to the server here
            // For example: sendData("PING");
            connectionLostLogged = false; // Đặt lại biến trạng thái khi kết nối được khôi phục
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
        }
        else {
            if (!connectionLostLogged) {
                if (logCallback) {
                    logCallback("STATUS:CONNECTION_LOST:" + currentServerIP);  // Format rõ ràng hơn
                }
                connectionLostLogged = true;
            }
            disconnect();
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
        }
    }
}