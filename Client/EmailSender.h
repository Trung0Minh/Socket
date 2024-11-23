// EmailSender.h
#pragma once
#include <string>
#include <vector>
#include "returnToken.h"

class EmailSender {
private:
    TokenManager tokenManager;

    // Callback functions
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // Email creation helpers
    std::string base64_encode(const std::string& str);
    std::string createEmailHeaders(const std::string& to,
        const std::string& subject,
        const std::string& boundary);
    std::string createEmailBody(const std::string& textContent,
        const std::string& boundary);
    std::string createAttachmentPart(const std::vector<uint8_t>& attachment,
        const std::string& attachmentName,
        const std::string& boundary);
    std::string createRawEmail(const std::string& to,
        const std::string& subject,
        const std::string& textContent,
        const std::vector<uint8_t>& attachment = std::vector<uint8_t>(),
        const std::string& attachmentName = "");

    // File handling
    std::vector<uint8_t> readFile(const std::string& filePath, std::string& fileName);

    // Email sending logic
    bool sendEmailRequest(const std::string& rawEmail);
    void handleEmailWithAttachment(const std::string& recipient,
        const std::string& subject,
        const std::string& body,
        const std::string& filePath);
    void handleSimpleEmail(const std::string& recipient,
        const std::string& subject,
        const std::string& body);

public:
    EmailSender() = default;
    ~EmailSender() = default;

    bool sendEmail(const std::string& to,
        const std::string& subject,
        const std::string& textContent,
        const std::vector<uint8_t>& attachment = std::vector<uint8_t>(),
        const std::string& attachmentName = "");

    void run();
};