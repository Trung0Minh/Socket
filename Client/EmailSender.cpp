// EmailSender.cpp
#include "EmailSender.h"
#include <sstream>
#include <iostream>
#include <curl/curl.h>
#include "base64.h"
#include <fstream>

// Callback function to append received data to a string
size_t EmailSender::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Encodes a string in base64
std::string EmailSender::base64_encode(const std::string& str) {
    return ::base64_encode(str, false);
}

// Creates the email headers with specified parameters
std::string EmailSender::createEmailHeaders(const std::string& to, const std::string& subject, const std::string& boundary) {
    std::stringstream headers;
    headers << "From: me\r\n"
        << "To: " << to << "\r\n"
        << "Subject: " << subject << "\r\n"
        << "MIME-Version: 1.0\r\n"
        << "Content-Type: multipart/mixed; boundary=" << boundary << "\r\n\r\n";
    return headers.str();
}

// Creates the email body with specified text content and boundary
std::string EmailSender::createEmailBody(const std::string& textContent, const std::string& boundary) {
    std::stringstream body;
    body << "--" << boundary << "\r\n"
        << "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
        << textContent << "\r\n\r\n";
    return body.str();
}

// Creates the attachment part with specified attachment data and name
std::string EmailSender::createAttachmentPart(const std::vector<uint8_t>& attachment, const std::string& attachmentName, const std::string& boundary) {
    if (attachment.empty() || attachmentName.empty()) {
        return "";
    }

    std::stringstream attachPart;
    attachPart << "--" << boundary << "\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "Content-Disposition: attachment; filename=\"" << attachmentName << "\"\r\n"
        << "Content-Transfer-Encoding: base64\r\n\r\n"
        << ::base64_encode(reinterpret_cast<const unsigned char*>(attachment.data()), attachment.size()) << "\r\n";
    return attachPart.str();
}

// Creates the raw email by combining headers, body, and attachment parts
std::string EmailSender::createRawEmail(const std::string& to, const std::string& subject, const std::string& textContent, const std::vector<uint8_t>& attachment, const std::string& attachmentName) {
    std::string boundary = "boundary_" + std::to_string(std::time(nullptr));
    std::stringstream email;

    // Combine all parts
    email << createEmailHeaders(to, subject, boundary)
        << createEmailBody(textContent, boundary)
        << createAttachmentPart(attachment, attachmentName, boundary)
        << "--" << boundary << "--\r\n";

    return base64_encode(email.str());
}

// Reads file content into a vector and extracts the file name
std::vector<uint8_t> EmailSender::readFile(const std::string& filePath, std::string& fileName) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // Extract filename from path
    size_t lastSlash = filePath.find_last_of("/\\");
    fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// Sends the email request using cURL with the raw email content
bool EmailSender::sendEmailRequest(const std::string& rawEmail) {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        std::cerr << "Failed to get valid access token" << std::endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    json payload = { {"raw", rawEmail} };
    std::string jsonStr = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, "https://gmail.googleapis.com/gmail/v1/users/me/messages/send");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);

    if (!success) {
        std::cerr << "Failed to send email: " << curl_easy_strerror(res) << std::endl;
        std::cerr << "Response: " << response << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

// Public function to send an email with specified parameters
bool EmailSender::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent,
    const std::vector<uint8_t>& attachment,
    const std::string& attachmentName) {
    try {
        std::string rawEmail = createRawEmail(to, subject, textContent, attachment, attachmentName);
        return sendEmailRequest(rawEmail);
    }
    catch (const std::exception& e) {
        std::cerr << "Error in sendEmail: " << e.what() << std::endl;
        return false;
    }
}