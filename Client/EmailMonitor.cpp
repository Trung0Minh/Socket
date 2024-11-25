#include "EmailMonitor.h"
#include "Client.h"
#include <iostream>
#include <curl/curl.h>
#include "base64.h"

using json = nlohmann::json;

size_t EmailMonitor::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

EmailMonitor::EmailMonitor(Client* client)
    : client(client),
    running(false) {
}

EmailMonitor::~EmailMonitor() {
    stop();
}

void EmailMonitor::start() {
    if (!running) {
        std::cout << "=== Starting Email Monitor ===" << std::endl;

        // Kiểm tra token file với thông báo chi tiết hơn
        std::ifstream tokenFile("token.json");
        if (!tokenFile.is_open()) {
            std::cerr << "Error: Cannot open token.json. Please ensure the file exists and has correct permissions" << std::endl;
            return;
        }

        std::string content((std::istreambuf_iterator<char>(tokenFile)),
            std::istreambuf_iterator<char>());

        if (content.empty()) {
            std::cerr << "Error: token.json is empty" << std::endl;
            tokenFile.close();
            return;
        }

        std::cout << "Token file loaded successfully" << std::endl;
        tokenFile.close();

        // Kiểm tra token với thông báo chi tiết
        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            std::cerr << "Error: Failed to get valid access token. Please check your credentials" << std::endl;
            return;
        }

        std::cout << "Access token validated successfully" << std::endl;
        running = true;
        monitorThread = std::thread(&EmailMonitor::monitorEmails, this);
        std::cout << "Monitor thread started successfully" << std::endl;
    }
    else {
        std::cout << "EmailMonitor is already running" << std::endl;
    }
}

void EmailMonitor::stop() {
    if (running) {
        std::cout << "Stopping EmailMonitor..." << std::endl;
        running = false;
        queueCV.notify_all();

        // Clear the email queue
        std::unique_lock<std::mutex> lock(queueMutex);
        std::queue<std::string> empty;
        std::swap(emailQueue, empty);
        lock.unlock();

        if (monitorThread.joinable()) {
            monitorThread.join();
        }
        std::cout << "EmailMonitor stopped successfully" << std::endl;
    }
}

bool EmailMonitor::checkNewEmails() {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        std::cerr << "Empty access token" << std::endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL init failed" << std::endl;
        return false;
    }

    std::string readBuffer;
    struct curl_slist* headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());

    std::string query = "is:unread subject:[Request]";
    char* encodedQuery = curl_easy_escape(curl, query.c_str(), 0);
    std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages?q=" + std::string(encodedQuery);

    curl_free(encodedQuery);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    try {
        json response = json::parse(readBuffer);
        if (response.contains("messages")) {
            auto messages = response["messages"];
            std::unique_lock<std::mutex> lock(queueMutex);
            for (const auto& message : messages) {
                emailQueue.push(message["id"]);
            }
            queueCV.notify_one();
            return true;
        }
    }
    catch (const json::exception& e) {
        return false;
    }

    return false;
}

std::string EmailMonitor::getEmailContent(const std::string& emailId) {
    int retryCount = 0;
    const int maxRetries = 2;

    while (retryCount < maxRetries) {
        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            return "";
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            return "";
        }

        std::string readBuffer;
        struct curl_slist* headers = NULL;
        std::string authHeader = "Authorization: Bearer " + accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + emailId;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK) {
            if (http_code == 401) {
                std::cout << "Token might be expired, retrying..." << std::endl;
                retryCount++;
                continue;
            }
            return "";
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return readBuffer;
    }
    return "";
}

bool EmailMonitor::markEmailAsRead(const std::string& emailId) {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string readBuffer;
    struct curl_slist* headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + emailId + "/modify";

    json payload;
    payload["removeLabelIds"] = json::array({ "UNREAD" });
    std::string jsonStr = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

std::string EmailMonitor::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");

    if (start == std::string::npos || end == std::string::npos)
        return "";

    return str.substr(start, end - start + 1);
}

EmailMonitor::EmailContent EmailMonitor::parseEmailContent(const nlohmann::json& emailData) {
    EmailContent content;

    try {
        // Extract headers with validation
        if (!emailData.contains("payload") || !emailData["payload"].contains("headers")) {
            throw std::runtime_error("Invalid email format: missing headers");
        }

        bool foundSubject = false, foundFrom = false;
        for (const auto& header : emailData["payload"]["headers"]) {
            if (header["name"] == "Subject") {
                content.subject = header["value"];
                foundSubject = true;
            }
            else if (header["name"] == "From") {
                content.from = header["value"];
                foundFrom = true;
            }
        }

        if (!foundSubject || !foundFrom) {
            throw std::runtime_error("Missing required headers");
        }

        // Extract and validate body
        if (!emailData["payload"].contains("parts")) {
            throw std::runtime_error("Invalid email format: missing body parts");
        }

        bool foundBody = false;
        for (const auto& part : emailData["payload"]["parts"]) {
            if (part["mimeType"] == "text/plain" && part["body"].contains("data")) {
                std::string encodedBody = part["body"]["data"];
                content.body = base64_decode(encodedBody);
                foundBody = true;
                break;
            }
        }

        if (!foundBody) {
            throw std::runtime_error("No plain text body found");
        }

        // Parse command and server IP with better validation
        size_t ipPos = content.body.find("SERVER_IP=");
        size_t cmdPos = content.body.find("COMMAND=");

        if (ipPos == std::string::npos || cmdPos == std::string::npos) {
            throw std::runtime_error("Missing SERVER_IP or COMMAND in email body");
        }

        size_t ipEndPos = content.body.find("\n", ipPos);
        size_t cmdEndPos = content.body.find("\n", cmdPos);

        content.serverIp = trim(content.body.substr(ipPos + 10, ipEndPos - (ipPos + 10)));
        content.command = trim(content.body.substr(cmdPos + 8, cmdEndPos - (cmdPos + 8)));

        if (content.serverIp.empty() || content.command.empty()) {
            throw std::runtime_error("Empty SERVER_IP or COMMAND value");
        }

        return content;
    }
    catch (const std::exception& e) {
        std::cerr << "Error parsing email content: " << e.what() << std::endl;
        return EmailContent(); // Return empty content
    }
}

void EmailMonitor::processEmail(const std::string& emailId) {
    std::string emailContentStr = getEmailContent(emailId);
    if (emailContentStr.empty()) {
        std::cerr << "Failed to get email content for ID: " << emailId << std::endl;
        return;
    }

    try {
        json emailData = json::parse(emailContentStr);
        EmailContent email = parseEmailContent(emailData);

        // Chỉ xử lý email có subject bắt đầu bằng [Request]
        if (email.subject.find("[Request]") == 0) {
            std::cout << "\n=== Request Email Found ===" << std::endl;
            std::cout << "From: " << email.from << std::endl;
            std::cout << "Subject: " << email.subject << std::endl;
            std::cout << "Body:\n" << email.body << std::endl;
            std::cout << "=========================" << std::endl;

            // Execute command if valid
            if (!email.command.empty() && !email.serverIp.empty()) {
                executeCommand(email);
            }

            // Call callback if exists
            if (callback) {
                callback(email.from, email.subject, email.body);
            }

            // Mark email as read after processing
            markEmailAsRead(emailId);
        }
    }
    catch (const json::exception& e) {
        std::cerr << "JSON parsing error while processing email: " << e.what() << std::endl;
    }
}

bool EmailMonitor::executeCommand(const EmailContent& email) {
    std::string response;
    bool success = false;

    try {
        // Thực hiện lệnh thông qua client
        success = client->executeCommand(email.serverIp, email.command, response);

        // Nếu thành công
        if (success) {
            std::string emailResponse = "Command execution successful.\nServer response: " + response;
            client->sendEmail(email.from,
                "Re: Command Execution Result",
                emailResponse);
        }
        // Nếu thất bại
        else {
            std::string emailResponse = "Failed to execute command on server " + email.serverIp + "\nError: " + response;
            client->sendEmail(email.from,
                "Re: Command Execution Failed",
                emailResponse);
        }
    }
    catch (const std::exception& e) {
        std::string emailResponse = std::string("Error executing command: ") + e.what();
        client->sendEmail(email.from,
            "Re: Command Execution Failed",
            emailResponse);
        success = false;
    }

    return success;
}

void EmailMonitor::monitorEmails() {
    std::cout << "=== Email monitoring started ===" << std::endl;

    const int CHECK_INTERVAL = 10; // seconds
    const int NETWORK_TIMEOUT = 30; // seconds

    while (running) {
        try {
            auto start = std::chrono::steady_clock::now();

            bool checkResult = checkNewEmails();
            if (!checkResult) {
                std::cout << "No new request emails found." << std::endl;
            }

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

            if (duration > NETWORK_TIMEOUT) {
                std::cerr << "Warning: Network operation took longer than expected (" << duration << " seconds)" << std::endl;
            }

            // Process emails in queue with timeout protection
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                while (!emailQueue.empty() && running) {
                    std::string emailId = emailQueue.front();
                    emailQueue.pop();
                    lock.unlock();

                    auto processStart = std::chrono::steady_clock::now();
                    processEmail(emailId);
                    auto processEnd = std::chrono::steady_clock::now();
                    auto processDuration = std::chrono::duration_cast<std::chrono::seconds>(processEnd - processStart).count();

                    if (processDuration > NETWORK_TIMEOUT) {
                        std::cerr << "Warning: Email processing took longer than expected (" << processDuration << " seconds)" << std::endl;
                    }

                    lock.lock();
                }
            }

            std::cout << "Waiting " << CHECK_INTERVAL << " seconds before next check..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
        }
        catch (const std::exception& e) {
            std::cerr << "Critical error in monitor loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait before retrying
        }
    }
}