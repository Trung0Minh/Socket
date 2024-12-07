#include "EmailMonitor.h"
#include <iostream>
#include <curl/curl.h>
#include "base64.h"

using json = nlohmann::json;

size_t EmailMonitor::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void EmailMonitor::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

void EmailMonitor::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    }
}

EmailMonitor::EmailMonitor() :
    running(false),
    callback(nullptr),
    logCallback(nullptr),
    commandExecutor(nullptr) {
}

EmailMonitor::~EmailMonitor() {
    stop();
}

void EmailMonitor::start() {
    if (!running) {
        std::ifstream tokenFile("token.json");
        if (!tokenFile.is_open()) {
            log("Error: Cannot open token.json. Please ensure the file exists and has correct permissions");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(tokenFile)), std::istreambuf_iterator<char>());
        if (content.empty()) {
            log("Error: token.json is empty");
            tokenFile.close();
            return;
        }

        log("Token file loaded successfully");
        tokenFile.close();

        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            log("Error: Failed to get valid access token. Please check your credentials");
            return;
        }

        log("Access token validated successfully");
        running = true;
        monitorThread = std::thread(&EmailMonitor::monitorEmails, this);
        log("Monitor thread started successfully");
    }
    else {
        log("EmailMonitor is already running");
    }
}

void EmailMonitor::stop() {
    if (running) {
        running = false;
        queueCV.notify_all();

        std::unique_lock<std::mutex> lock(queueMutex);
        std::queue<std::string> empty;
        std::swap(emailQueue, empty);
        lock.unlock();

        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }
}

bool EmailMonitor::checkNewEmails() {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        log("Error: Empty access token");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        log("Error: CURL init failed");
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        log("Error: Failed to perform CURL request: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    try {
        json response = json::parse(readBuffer);
        if (response.contains("messages")) {
            std::unique_lock<std::mutex> lock(queueMutex);
            for (const auto& message : response["messages"]) {
                emailQueue.push(message["id"]);
            }
            queueCV.notify_one();
            return true;
        }
    }
    catch (const json::exception& e) {
        log("Error: Failed to parse JSON response: " + std::string(e.what()));
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
            log("Error: Empty access token");
            return "";
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            log("Error: CURL init failed");
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK) {
            log("Error: Failed to perform CURL request: " + std::string(curl_easy_strerror(res)));
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (http_code == 401) {
                log("Token might be expired, retrying...");
                retryCount++;
                continue;
            }
            return "";
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return readBuffer;
    }

    log("Error: Maximum retries exceeded for getting email content");
    return "";
}

bool EmailMonitor::markEmailAsRead(const std::string& emailId) {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        log("Error: Empty access token");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        log("Error: CURL init failed");
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        log("Error: Failed to mark email as read: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    return true;
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
        log("Error parsing email content: " + std::string(e.what()));
        return EmailContent();
    }
}

void EmailMonitor::processEmail(const std::string& emailId) {
    try {
        auto emailData = nlohmann::json::parse(getEmailContent(emailId));
        EmailContent content = parseEmailContent(emailData);

        if (callback) {
            callback(content.from, content.subject, content.body);
        }

        if (!content.command.empty() && commandExecutor) {
            std::string response;
            if (!commandExecutor(content.serverIp, content.command, response, content.from)) {
                log("Error: Command execution failed");
            }
        }

        if (!markEmailAsRead(emailId)) {
            log("Warning: Failed to mark email as read");
        }
    }
    catch (const std::exception& e) {
        log("Error processing email: " + std::string(e.what()));
    }
}

void EmailMonitor::monitorEmails() {
    const int CHECK_INTERVAL = 10;
    const int NETWORK_TIMEOUT = 30;

    while (running) {
        try {
            auto start = std::chrono::steady_clock::now();
            checkNewEmails();

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

            if (duration > NETWORK_TIMEOUT) {
                log("Warning: Network operation took longer than expected (" + std::to_string(duration) + " seconds)");
            }

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
                        log("Warning: Email processing took longer than expected (" + std::to_string(processDuration) + " seconds)");
                    }

                    lock.lock();
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
        }
        catch (const std::exception& e) {
            log("Critical error in monitor loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}