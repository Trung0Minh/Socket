// EmailSender.cpp
#include "EmailSender.h"
#include <sstream>
#include <iostream>
#include <curl/curl.h>
#include "base64.h"
#include <fstream>

size_t EmailSender::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string EmailSender::base64_encode(const std::string& str) {
    return ::base64_encode(str, false);
}

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

std::string EmailSender::createEmailBody(const std::string& textContent,
    const std::string& boundary) {
    std::stringstream body;
    body << "--" << boundary << "\r\n"
        << "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
        << textContent << "\r\n\r\n";
    return body.str();
}

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
        << ::base64_encode(reinterpret_cast<const unsigned char*>(attachment.data()),
            attachment.size()) << "\r\n";
    return attachPart.str();
}

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

std::vector<uint8_t> EmailSender::readFile(const std::string& filePath, std::string& fileName) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // Extract filename from path
    size_t lastSlash = filePath.find_last_of("/\\");
    fileName = (lastSlash != std::string::npos) ?
        filePath.substr(lastSlash + 1) : filePath;

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

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

void EmailSender::handleEmailWithAttachment(const std::string& recipient,
    const std::string& subject,
    const std::string& body,
    const std::string& filePath) {
    try {
        std::string fileName;
        std::vector<uint8_t> fileContent = readFile(filePath, fileName);
        if (sendEmail(recipient, subject, body, fileContent, fileName)) {
            std::cout << "Email with attachment sent successfully\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing file: " << e.what() << std::endl;
    }
}

void EmailSender::handleSimpleEmail(const std::string& recipient,
    const std::string& subject,
    const std::string& body) {
    if (sendEmail(recipient, subject, body)) {
        std::cout << "Email sent successfully\n";
    }
}

bool EmailSender::sendEmail(const std::string& to,
    const std::string& subject,
    const std::string& textContent,
    const std::vector<uint8_t>& attachment,
    const std::string& attachmentName) {
    std::string rawEmail = createRawEmail(to, subject, textContent, attachment, attachmentName);
    return sendEmailRequest(rawEmail);
}

void EmailSender::run() {
    while (true) {
        std::cout << "\nEmail Sender Menu:\n"
            << "1. Send email with text file\n"
            << "2. Send email with image\n"
            << "3. Send simple email\n"
            << "4. Exit\n"
            << "Choose an option: ";

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "4") break;

        std::string recipient, subject, body, filePath;

        std::cout << "Enter recipient email: ";
        std::getline(std::cin, recipient);

        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);

        std::cout << "Enter body: ";
        std::getline(std::cin, body);

        if (choice == "1" || choice == "2") {
            std::cout << "Enter file path: ";
            std::getline(std::cin, filePath);
            handleEmailWithAttachment(recipient, subject, body, filePath);
        }
        else if (choice == "3") {
            handleSimpleEmail(recipient, subject, body);
        }
    }
}