// EmailSender.h
#pragma once
#include <string>
#include <vector>
#include "returnToken.h"

class EmailSender {
private:
    TokenManager tokenManager;

    // Callback function for receiving data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // Functions for creating different parts of the email
    std::string base64_encode(const std::string& str);
    std::string createEmailHeaders(const std::string& to, 
        const std::string& subject, 
        const std::string& boundary);
    std::string createEmailBody(const std::string& textContent, const std::string& boundary);
    std::string createAttachmentPart(const std::vector<uint8_t>& attachment, 
        const std::string& attachmentName, 
        const std::string& boundary);
    std::string createRawEmail(const std::string& to, 
        const std::string& subject, 
        const std::string& textContent, 
        const std::vector<uint8_t>& attachment = std::vector<uint8_t>(), 
        const std::string& attachmentName = "");

    // Logic for sending the email request
    bool sendEmailRequest(const std::string& rawEmail);

    bool parseHeader(const std::string& textContent, std::string& type, std::string& body, std::string& filename, std::string& mimetype);

public:
    EmailSender() = default;
    ~EmailSender() = default;

    // Public function to send an email
    bool sendEmail(const std::string& to,
        const std::string& subject,
        const std::string& textContent);
};