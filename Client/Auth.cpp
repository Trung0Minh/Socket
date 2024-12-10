#include "Auth.h"
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include "json.hpp"
#include "AuthUI.h"

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    s->append((char*)contents, newLength);
    return newLength;
}

Auth::Auth(const std::string& credentialsFile, const std::string& tokenFile)
    : credentialsFile(credentialsFile), tokenFile(tokenFile) {
    token = loadToken();
    if (!hasValidToken()) {
        nlohmann::json credentials = loadCredentials();
        clientId = credentials["installed"]["client_id"];
        clientSecret = credentials["installed"]["client_secret"];
        authUri = credentials["installed"]["auth_uri"];
        tokenUri = credentials["installed"]["token_uri"];
    }
}

bool Auth::authenticate() {
    if (hasValidToken()) {
        return true;
    }

    authenticationCancelled = false;
    requestAuthorizationCode();
    return hasValidToken();
}

void Auth::requestAuthorizationCode() {
    std::string authUrl = authUri +
        "?client_id=" + clientId +
        "&redirect_uri=urn:ietf:wg:oauth:2.0:oob" +
        "&scope=https://mail.google.com/" +
        "&response_type=code";

    // Tạo một temporary frame làm parent cho dialog
    wxFrame* tempFrame = new wxFrame(nullptr, wxID_ANY, "");
    AuthDialog* dialog = new AuthDialog(tempFrame, wxString(authUrl));

    if (dialog->ShowModal() == wxID_OK) {
        try {
            std::string authCode = dialog->GetAuthorizationCode();
            token = exchangeAuthorizationCodeForTokens(authCode);
            saveToken(token);
        }
        catch (const std::exception& e) {
            dialog->Destroy();
            tempFrame->Destroy();
            throw; // Re-throw nếu có lỗi trong quá trình xử lý authorization code
        }
    }
    else {
        authenticationCancelled = true;
    }

    dialog->Destroy();
    tempFrame->Destroy();
}

nlohmann::json Auth::exchangeAuthorizationCodeForTokens(const std::string& authCode) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    char errorBuffer[CURL_ERROR_SIZE];

    curl = curl_easy_init();
    if (curl) {
        std::string postFields = "code=" + authCode +
            "&client_id=" + clientId +
            "&client_secret=" + clientSecret +
            "&redirect_uri=urn:ietf:wg:oauth:2.0:oob" +
            "&grant_type=authorization_code";

        curl_easy_setopt(curl, CURLOPT_URL, tokenUri.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

        // Disable SSL certificate verification (only for testing)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("Failed to get tokens");
        }

        curl_easy_cleanup(curl);
    }

    try {
        return json::parse(readBuffer);
    }
    catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse server response");
    }
}

nlohmann::json Auth::loadCredentials() {
    std::ifstream file(credentialsFile);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open credentials file");
    }
    json credentials;
    file >> credentials;

    return credentials;
}

nlohmann::json Auth::loadToken() {
    std::ifstream file(tokenFile);
    if (!file.is_open()) {
        return nlohmann::json();
    }
    json j;
    file >> j;
    return j;
}

void Auth::saveToken(const nlohmann::json& token) {
    std::ofstream file(tokenFile);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file for writing tokens");
    }
    file << token.dump(4);
}

bool Auth::hasValidToken() {
    return token.contains("access_token") && token.contains("refresh_token");
}

std::string Auth::getAuthUrl() const {
    return authUri +
        "?client_id=" + clientId +
        "&redirect_uri=urn:ietf:wg:oauth:2.0:oob" +
        "&scope=https://mail.google.com/" +
        "&response_type=code";
}

void Auth::processAuthorizationCode(const std::string& authCode) {
    token = exchangeAuthorizationCodeForTokens(authCode);
    saveToken(token);
}