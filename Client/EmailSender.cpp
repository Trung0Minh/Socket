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
std::string EmailSender::createEmailHeaders(const std::string& to,
    const std::string& subject,
    const std::string& boundary) {
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
std::string EmailSender::createAttachmentPart(const std::vector<uint8_t>& attachment,
    const std::string& attachmentName,
    const std::string& boundary) {
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
std::string EmailSender::createRawEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent,
    const std::vector<uint8_t>& attachment,
    const std::string& attachmentName) {
    std::string boundary = "boundary_" + std::to_string(std::time(nullptr));
    std::stringstream email;

    // Combine all parts
    email << createEmailHeaders(to, subject, boundary)
        << createEmailBody(textContent, boundary)
        << createAttachmentPart(attachment, attachmentName, boundary)
        << "--" << boundary << "--\r\n";

    return base64_encode(email.str());
}

// Function for parsing header from textContent
bool EmailSender::parseHeader(const std::string& textContent, std::string& type, std::string& body, std::string& filename, std::string& mimetype) {
    // Tìm vị trí ký tự newline đầu tiên một cách an toàn
    size_t headerEndPos = 0;
    for (size_t i = 0; i < textContent.size(); i++) {
        if (textContent[i] == '\n') {
            headerEndPos = i;
            break;
        }
    }
    if (headerEndPos == 0) return false;

    // Tách header và body
    std::string header = textContent.substr(0, headerEndPos);
    body = textContent.substr(headerEndPos + 1);

    // Parse các trường trong header
    std::map<std::string, std::string> headerFields;
    std::stringstream ss(header);
    std::string field;
    while (std::getline(ss, field, '|')) {
        size_t colonPos = field.find(':');
        if (colonPos != std::string::npos) {
            std::string key = field.substr(0, colonPos);
            std::string value = field.substr(colonPos + 1);
            headerFields[key] = value;
        }
    }

    // Kiểm tra các trường bắt buộc
    if (headerFields.find("TYPE") == headerFields.end() ||
        headerFields.find("SIZE") == headerFields.end()) {
        return false;
    }

    type = headerFields["TYPE"];
    int expectedSize = std::stoi(headerFields["SIZE"]);

    // Kiểm tra kích thước body
    if (body.size() != static_cast<size_t>(expectedSize)) {
        return false;
    }

    // Lấy filename và mimetype nếu có
    if (headerFields.find("FILENAME") != headerFields.end()) {
        filename = headerFields["FILENAME"];
    }
    if (headerFields.find("MIMETYPE") != headerFields.end()) {
        mimetype = headerFields["MIMETYPE"];
    }

    return true;
}

// Sends the email request using cURL with the raw email content
bool EmailSender::sendEmailRequest(const std::string& rawEmail) {
    std::string accessToken = tokenManager.getValidAccessToken();
    if (accessToken.empty()) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
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

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

// Public function to send an email with specified parameters
bool EmailSender::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent) {
    try {
        std::string type;
        std::string body;
        std::string filename;
        std::string mimetype;

        if (!parseHeader(textContent, type, body, filename, mimetype)) {
            std::cerr << "Failed to parse header" << std::endl;
            return false;
        }

        std::string rawEmail;
        if (type == "text") {
            // Gửi email text bình thường
            rawEmail = createRawEmail(to, subject, body);
        }
        else if (type == "file") {
            if (filename.empty() || mimetype.empty()) {
                std::cerr << "Missing filename or MIME type for file" << std::endl;
                return false;
            }

            // Tạo vector chứa binary data
            std::vector<uint8_t> fileData(body.begin(), body.end());

            // Tạo subject phù hợp với loại file
            std::string enhancedSubject = subject;
            if (mimetype.find("image") != std::string::npos) {
                enhancedSubject += " [Image Attachment]";
            }
            else if (mimetype.find("video") != std::string::npos) {
                enhancedSubject += " [Video Attachment]";
            }

            // Tạo nội dung mô tả
            std::string description = "Attached file: " + filename + "\nType: " + mimetype;

            // Gửi email với attachment
            rawEmail = createRawEmail(to, enhancedSubject, description, fileData, filename);
        }
        else {
            std::cerr << "Unsupported type: " << type << std::endl;
            return false;
        }

        if (rawEmail.empty()) {
            std::cerr << "Failed to create email content" << std::endl;
            return false;
        }

        bool success = sendEmailRequest(rawEmail);
        if (!success) {
            std::cerr << "Failed to send email request" << std::endl;
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in sendEmail: " << e.what() << std::endl;
        return false;
    }
}