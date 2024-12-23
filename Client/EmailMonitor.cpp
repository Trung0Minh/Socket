#include "EmailMonitor.h"
#include <iostream>
#include <curl/curl.h>
#include <regex>
#include "base64.h"

using json = nlohmann::json;
using namespace std::chrono_literals;

size_t EmailMonitor::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
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
        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            log("Error: Failed to get valid access token. Please check your credentials");
            return;
        }

        running = true;
        monitorThread = std::thread(&EmailMonitor::monitorEmails, this);
    }
}

void EmailMonitor::stop() {
    if (running) {
        running = false;
        queueCV.notify_all();

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::queue<std::string>().swap(emailQueue);
        }

        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }
}

EmailMonitor::CurlResponse EmailMonitor::performCurlRequest(const std::string& url,
    const std::string& accessToken,
    const std::string& method,
    const std::string& postData) {
    CurlResponse response{};
    response.success = false;

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        log("Error: CURL init failed");
        return response;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
    if (!postData.empty()) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.data);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);

    if (method != "GET") {
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!postData.empty()) {
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, postData.c_str());
        }
    }

    response.success = (curl_easy_perform(curl.get()) == CURLE_OK);
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response.httpCode);

    curl_slist_free_all(headers);
    return response;
}

bool EmailMonitor::checkNewEmails() {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        log("Error: Empty access token");
        return false;
    }

    std::string query = "is:unread subject:[Request]";
    std::unique_ptr<char, decltype(&curl_free)> encodedQuery(
        curl_easy_escape(nullptr, query.c_str(), 0),
        curl_free
    );

    std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages?q=" +
        std::string(encodedQuery.get());

    auto response = performCurlRequest(url, accessToken);
    if (!response.success) return false;

    try {
        auto jsonResponse = json::parse(response.data);
        if (jsonResponse.contains("messages")) {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (const auto& message : jsonResponse["messages"]) {
                emailQueue.push(message["id"]);
            }
            queueCV.notify_one();
            return true;
        }
    }
    catch (const json::exception& e) {
        log("Error: Failed to parse JSON response: " + std::string(e.what()));
    }
    return false;
}

std::string EmailMonitor::getEmailContent(const std::string& emailId) {
    const int maxRetries = 2;
    for (int retryCount = 0; retryCount < maxRetries; ++retryCount) {
        std::string accessToken = tokenManager.getValidAccessToken();
        if (accessToken.empty()) {
            log("Error: Empty access token");
            continue;
        }

        std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + emailId;
        auto response = performCurlRequest(url, accessToken);

        if (!response.success) {
            if (response.httpCode == 401) {
                log("Token might be expired, retrying...");
                continue;
            }
            break;
        }
        return response.data;
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

    std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" +
        emailId + "/modify";

    json payload;
    payload["removeLabelIds"] = json::array({ "UNREAD" });

    auto response = performCurlRequest(url, accessToken, "POST", payload.dump());
    return response.success;
}

std::string EmailMonitor::trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    const auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

EmailMonitor::EmailContent EmailMonitor::parseEmailContent(const nlohmann::json& emailData) {
    EmailContent content;
    try {
        const auto& headers = emailData["payload"]["headers"];
        auto findHeader = [&headers](const std::string& name) {
            auto it = std::find_if(headers.begin(), headers.end(),
                [&name](const auto& header) { return header["name"] == name; });
            return it != headers.end() ? (*it)["value"].get<std::string>() : "";
            };

        content.subject = findHeader("Subject");
        content.from = findHeader("From");

        if (content.subject.empty() || content.from.empty()) {
            throw std::runtime_error("Missing required headers");
        }

        for (const auto& part : emailData["payload"]["parts"]) {
            if (part["mimeType"] == "text/plain") {
                content.body = base64_decode(part["body"]["data"].get<std::string>());
                break;
            }
        }

        std::regex pattern(R"(SERVER_IP=([^\n]+)\s*COMMAND=([^\n]+))");
        std::smatch matches;
        if (std::regex_search(content.body, matches, pattern)) {
            content.serverIp = trim(matches[1].str());
            content.command = trim(matches[2].str());
        }

        if (content.serverIp.empty() || content.command.empty()) {
            throw std::runtime_error("Missing or invalid SERVER_IP or COMMAND");
        }
    }
    catch (const std::exception& e) {
        log("Error parsing email: " + std::string(e.what()));
    }
    return content;
}

void EmailMonitor::monitorEmails() {
    const auto CHECK_INTERVAL = 1s;
    const auto NETWORK_TIMEOUT = 60s;

    while (running) {
        try {
            auto start = std::chrono::steady_clock::now();

            if (checkNewEmails()) {
                std::unique_lock<std::mutex> lock(queueMutex);
                while (!emailQueue.empty() && running) {
                    auto emailId = std::move(emailQueue.front());
                    emailQueue.pop();
                    lock.unlock();

                    processEmail(emailId);

                    lock.lock();
                }
            }

            auto duration = std::chrono::steady_clock::now() - start;
            if (duration > NETWORK_TIMEOUT) {
                log("Warning: Operation took " +
                    std::to_string(std::chrono::duration_cast<std::chrono::seconds>(duration).count()) +
                    " seconds");
            }

            std::this_thread::sleep_for(CHECK_INTERVAL);
        }
        catch (const std::exception& e) {
            log("Critical error: " + std::string(e.what()));
            std::this_thread::sleep_for(5s);
        }
    }
}

void EmailMonitor::processEmail(const std::string& emailId) {
    std::string emailContent = getEmailContent(emailId);
    if (emailContent.empty()) {
        log("Error: Failed to get email content");
        return;
    }

    try {
        auto emailData = json::parse(emailContent);
        auto content = parseEmailContent(emailData);

        log("Command '" + content.command + "' is requested to " + content.serverIp);

        std::string output;
        bool shouldMarkAsRead = false;
        if (commandExecutor && commandExecutor(content.serverIp, content.command, output, content.from, shouldMarkAsRead)) {
            if (callback) {
                callback(content.from, content.command, output);
            }
        }

        // Kiểm tra biến shouldMarkAsRead để quyết định có đánh dấu email là đã đọc hay không
        if (shouldMarkAsRead) {
            markEmailAsRead(emailId);
        }
    }
    catch (const json::exception& e) {
        log("Error parsing email JSON: " + std::string(e.what()));
        markEmailAsRead(emailId); // Đánh dấu là đã đọc khi có lỗi parse
    }
    catch (const std::exception& e) {
        log("Error processing email: " + std::string(e.what()));
        markEmailAsRead(emailId); // Đánh dấu là đã đọc khi có lỗi xử lý
    }
}