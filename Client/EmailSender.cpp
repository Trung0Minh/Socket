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
    size_t headerEndPos = textContent.find('\n');
    if (headerEndPos == std::string::npos) {
        return false;
    }

    std::string header = textContent.substr(0, headerEndPos);
    body = textContent.substr(headerEndPos + 1);

    size_t typePos = header.find("TYPE:");
    size_t sizePos = header.find("SIZE:");
    size_t filenamePos = header.find("FILENAME:");
    size_t mimetypePos = header.find("MIMETYPE:");

    if (typePos == std::string::npos || sizePos == std::string::npos) {
        return false;
    }

    typePos += 5;  // Skip "TYPE:"
    sizePos += 5;  // Skip "SIZE:"

    size_t endTypePos = header.find('|', typePos);
    size_t endSizePos = header.find('|', sizePos);

    if (endTypePos == std::string::npos || endSizePos == std::string::npos) {
        return false;
    }

    type = header.substr(typePos, endTypePos - typePos);
    std::string sizeStr = header.substr(sizePos, endSizePos - sizePos);
    int dataSize = std::stoi(sizeStr);

    if (body.size() != static_cast<size_t>(dataSize)) {
        return false;
    }

    if (filenamePos != std::string::npos && mimetypePos != std::string::npos) {
        filenamePos += 9;  // Skip "FILENAME:"
        mimetypePos += 9;  // Skip "MIMETYPE:"

        size_t endFilenamePos = header.find('|', filenamePos);
        size_t endMimetypePos = header.find('\n', mimetypePos);

        if (endFilenamePos == std::string::npos || endMimetypePos == std::string::npos) {
            return false;
        }

        filename = header.substr(filenamePos, endFilenamePos - filenamePos);
        mimetype = header.substr(mimetypePos, endMimetypePos - mimetypePos);
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
            std::cerr << "Invalid header format" << std::endl;
            return false;
        }

        std::string rawEmail;
        if (type == "text") {
            rawEmail = createRawEmail(to, subject, body);
        }
        else if (type == "file") {
            std::vector<uint8_t> attachment(body.begin(), body.end());
            rawEmail = createRawEmail(to, subject, "", attachment, filename);
        }
        else {
            std::cerr << "Unsupported TYPE in header" << std::endl;
            return false;
        }

        return sendEmailRequest(rawEmail);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}