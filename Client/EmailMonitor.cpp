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

        // Check token file
        std::ifstream tokenFile("token.json");
        if (tokenFile.is_open()) {
            std::string content((std::istreambuf_iterator<char>(tokenFile)),
                std::istreambuf_iterator<char>());
            std::cout << "Token file content: " << content << std::endl;
            tokenFile.close();
        }
        else {
            std::cerr << "Cannot open token.json" << std::endl;
            return;
        }

        // Check if token is valid
        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            std::cerr << "Failed to get valid access token" << std::endl;
            return;
        }

        running = true;
        monitorThread = std::thread(&EmailMonitor::monitorEmails, this);
        std::cout << "Monitor thread started" << std::endl;
    }
}

void EmailMonitor::stop() {
    if (running) {
        running = false;
        queueCV.notify_all();
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
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

    // Extract headers
    if (emailData.contains("payload") && emailData["payload"].contains("headers")) {
        for (const auto& header : emailData["payload"]["headers"]) {
            if (header["name"] == "Subject") {
                content.subject = header["value"];
            }
            else if (header["name"] == "From") {
                content.from = header["value"];
            }
        }
    }

    // Extract body
    if (emailData.contains("payload") && emailData["payload"].contains("parts")) {
        for (const auto& part : emailData["payload"]["parts"]) {
            if (part["mimeType"] == "text/plain") {
                std::string encodedBody = part["body"]["data"];
                content.body = base64_decode(encodedBody);
                break;
            }
        }
    }

    // Parse command and server IP from body
    size_t ipPos = content.body.find("SERVER_IP=");
    size_t cmdPos = content.body.find("COMMAND=");

    if (ipPos != std::string::npos && cmdPos != std::string::npos) {
        size_t ipEndPos = content.body.find("\n", ipPos);
        content.serverIp = trim(content.body.substr(ipPos + 10, ipEndPos - (ipPos + 10)));

        size_t cmdEndPos = content.body.find("\n", cmdPos);
        content.command = trim(content.body.substr(cmdPos + 8, cmdEndPos - (cmdPos + 8)));
    }

    return content;
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
    bool success = client->executeCommand(email.serverIp, email.command, response);

    std::string emailResponse = success ?
        "Command execution successful.\nServer response: " + response :
        "Failed to execute command on server " + email.serverIp;

    client->sendEmail(email.from,
        "Re: Command Execution Result",
        emailResponse);

    return success;
}

void EmailMonitor::monitorEmails() {
    std::cout << "=== Email monitoring started ===" << std::endl;
    while (running) {
        try {
            bool checkResult = checkNewEmails();
            if (!checkResult) {
                std::cout << "No new request emails found." << std::endl;
            }
            else {
                // Process any emails in the queue
                std::unique_lock<std::mutex> lock(queueMutex);
                while (!emailQueue.empty()) {
                    std::string emailId = emailQueue.front();
                    emailQueue.pop();
                    lock.unlock(); // Unlock while processing email

                    processEmail(emailId); // Process the email

                    lock.lock(); // Lock again to check queue
                }
            }

            std::cout << "Waiting 10 seconds before next check..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        catch (const std::exception& e) {
            std::cerr << "Error in monitor loop: " << e.what() << std::endl;
        }
    }
}