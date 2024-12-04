#include "Client.h"
#include <iostream>
#include <sstream>
#include <fstream>

// Constructor initializes members and sets up email monitor callback
Client::Client() :
    clientSocket(std::make_unique<ClientSocket>()),
    emailMonitor(std::make_unique<EmailMonitor>()),
    emailSender(), // Đảm bảo emailSender được khởi tạo
    running(false),
    shouldCheckConnection(false),
    reconnectionAttempts(0),
    needReconnection(false),
    logCallback(nullptr) {

    // Thiết lập command executor
    emailMonitor->setCommandExecutor(
        [this](const std::string& serverIP, const std::string& command,
            std::string& response, const std::string& senderEmail) -> bool {
                try {
                    if (serverIP.empty() || command.empty()) {
                        log("Error: Invalid command parameters");
                        return false;
                    }

                    lastSenderEmail = senderEmail; // Lưu email người gửi

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

// Destructor stops the client and joins threads
Client::~Client() {
    stop();
}

// Sets the logging callback function
void Client::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
    // Đồng bộ logging system
    if (emailMonitor) {
        emailMonitor->setLogCallback(
            [this](const std::string& message) {
                if (logCallback) logCallback(message);
            }
        );
    }
}

// Helper function to log messages using the callback
void Client::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    }
}

// Sets the email monitoring callback
void Client::setEmailCallback(EmailMonitor::EmailCallback cb) {
    if (emailMonitor) {
        emailMonitor->setCallback(cb);
    }
}

// Starts the email monitor
void Client::startEmailMonitor() {
    if (emailMonitor) {
        emailMonitor->start();
    }
}

// Stops the email monitor
void Client::stopEmailMonitor() {
    if (emailMonitor) {
        emailMonitor->stop();
        log("Email monitor stopped");
    }
}

// Checks if the email monitor is running
bool Client::isEmailMonitorRunning() const {
    return emailMonitor && emailMonitor->isRunning();
}

// Connects to the server with given IP and port
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
    reconnectionAttempts = 0;
    shouldCheckConnection = true;
    return true;
}

// Disconnects from the server
void Client::disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex);

    if (clientSocket) {
        clientSocket->Close();
        clientSocket = std::make_unique<ClientSocket>();
    }

    currentServerIP.clear();
    currentPort.clear();
    reconnectionAttempts = 0;
}

// Checks if the client is connected to the server
bool Client::isConnected() const {
    return clientSocket && clientSocket->IsConnected();
}

// Sends data to the server
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

// Receives data from the server
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

// Executes a command on the server
bool Client::executeCommand(const std::string& serverIP,
    const std::string& command,
    std::string& response,
    const std::string& senderEmail) {

    try {
        if (!isConnected()) {
            log("Lost connection to server, attempting to reconnect...");
            if (!connect(serverIP)) {
                if (reconnectCallback) {
                    reconnectCallback("Reconnection failed to " + serverIP);
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

        // Gửi email phản hồi nếu có người gửi
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

// Sends an email with optional attachment
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

// Starts the client and connection checker thread
bool Client::start(const std::string& serverIP) {
    if (!isConnected()) {
        if (!connect(serverIP, "54000")) {
            log("Failed to connect during startup");
            return false;
        }
    }

    running = true;
    shouldCheckConnection = true;

    // Thêm dòng này để EmailMonitor dùng cùng logging system
    emailMonitor->setLogCallback(logCallback);

    connectionCheckerThread = std::thread(&Client::connectionCheckerLoop, this);
    startEmailMonitor();
    log("Client started successfully");
    return true;
}

// Stops the client and cleans up resources
void Client::stop() {
    if (running) {
        running = false;
        shouldCheckConnection = false;

        if (connectionCheckerThread.joinable()) {
            connectionCheckerThread.join();
        }

        stopEmailMonitor();
        disconnect();
        log("Client stopped successfully");
    }
}

// Connection checking loop for reconnection attempts
void Client::connectionCheckerLoop() {
    const int ATTEMPT_DURATION = 5; // Thời gian mỗi lần attempt (giây)
    bool hasLoggedConnectionLost = false;

    while (shouldCheckConnection && running) {
        if (!clientSocket->IsConnected()) {
            if (!hasLoggedConnectionLost) {
                log("Connection lost to server at " + currentServerIP + ":" + currentPort);
                hasLoggedConnectionLost = true;
            }

            // Thử kết nối lại
            if (reconnectionAttempts < MAX_RECONNECTION_ATTEMPTS) {
                std::lock_guard<std::mutex> lock(socketMutex);
                reconnectionAttempts++;

                log("Reconnection attempt " + std::to_string(reconnectionAttempts) +
                    " of " + std::to_string(MAX_RECONNECTION_ATTEMPTS));

                // Attempt reconnection for 5 seconds with loading animation
                bool reconnected = false;
                int dotCount = 0;
                auto startTime = std::chrono::steady_clock::now();

                while (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count() < ATTEMPT_DURATION) {

                    // Attempt connection
                    if (clientSocket->Connect(currentServerIP.c_str(), currentPort.c_str(), AF_INET)) {
                        reconnected = true;
                        break;
                    }
                }

                if (reconnected) {
                    log("Successfully reconnected to server");
                    reconnectionAttempts = 0; // Reset counter on successful reconnection
                    hasLoggedConnectionLost = false;

                    // Thêm gọi callback khi reconnect thành công
                    if (reconnectCallback) {
                        reconnectCallback(currentServerIP);
                    }
                }
                else {
                    log("Reconnection attempt " + std::to_string(reconnectionAttempts) + " failed");
                }
            }
            else {
                log("Maximum reconnection attempts reached. Connection to server " +
                    currentServerIP + " is completely lost");
                log("Please reconnect manually with new server IP");

                // Reset connection-related variables
                currentServerIP.clear();
                currentPort.clear();
                reconnectionAttempts = 0;
                hasLoggedConnectionLost = false;

                // Stop the connection checker
                shouldCheckConnection = false;
                break;
            }
        }
        else {
            hasLoggedConnectionLost = false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Sets the callback for reconnection events
void Client::setReconnectCallback(std::function<void(const std::string&)> callback) {
    reconnectCallback = callback;
}