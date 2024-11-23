// returnToken.cpp
#include "returnToken.h"

// Add static keyword here
size_t TokenManager::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool TokenManager::isTokenExpired() {
    try {
        if (!token_data.contains("created_at")) {
            token_data["created_at"] = time(nullptr);
            return true;
        }

        time_t now = time(nullptr);
        time_t token_created = token_data["created_at"].get<time_t>();
        int expires_in = token_data["expires_in"].get<int>();
        return (now - token_created) >= expires_in;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in isTokenExpired: " << e.what() << std::endl;
        return true;
    }
}

bool TokenManager::refreshToken() {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        std::string response;
        std::string post_data = "client_id=" + config->CLIENT_ID +
            "&client_secret=" + config->CLIENT_SECRET +
            "&refresh_token=" + token_data["refresh_token"].get<std::string>() +
            "&grant_type=refresh_token";

        curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);

        bool success = (res == CURLE_OK);
        if (success) {
            json new_token = json::parse(response);
            token_data["access_token"] = new_token["access_token"];
            token_data["created_at"] = std::time(nullptr);
            token_data["expires_in"] = new_token["expires_in"];
            std::ofstream file(config->TOKEN_FILE);
            file << token_data.dump(4);
        }
        else {
            std::cerr << "Failed to refresh token: " << curl_easy_strerror(res) << std::endl;
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return success;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in refreshToken: " << e.what() << std::endl;
        return false;
    }
}

bool TokenManager::loadTokenFile() {
    try {
        std::ifstream file(config->TOKEN_FILE);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << config->TOKEN_FILE << std::endl;
            return false;
        }

        std::string json_str;
        std::string line;
        while (std::getline(file, line)) {
            json_str += line;
        }
        file.close();

        if (json_str.empty()) {
            std::cerr << "Empty token file" << std::endl;
            return false;
        }

        token_data = json::parse(json_str);

        if (!token_data.contains("access_token") ||
            !token_data.contains("expires_in") ||
            !token_data.contains("refresh_token")) {
            std::cerr << "Missing required fields in token file" << std::endl;
            return false;
        }

        if (!token_data.contains("created_at")) {
            token_data["created_at"] = time(nullptr);
            std::ofstream out_file(config->TOKEN_FILE);
            out_file << token_data.dump(4);
            out_file.close();
        }

        return true;
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in loadTokenFile: " << e.what() << std::endl;
        return false;
    }
}

std::string TokenManager::getValidAccessToken() {
    try {
        if (!loadTokenFile()) {
            std::cerr << "Failed to load token file" << std::endl;
            return "";
        }

        if (isTokenExpired()) {
            std::cout << "Token is expired" << std::endl;
            if (!refreshToken()) {
                std::cerr << "Failed to refresh token" << std::endl;
                return "";
            }
        }

        return token_data["access_token"].get<std::string>();
    }
    catch (const std::exception& e) {
        std::cerr << "Error in getValidAccessToken: " << e.what() << std::endl;
        return "";
    }
}

void TokenManager::printTokenData() {
    try {
        std::cout << "Token Data:" << std::endl;
        std::cout << token_data.dump(4) << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error printing token data: " << e.what() << std::endl;
    }
}
