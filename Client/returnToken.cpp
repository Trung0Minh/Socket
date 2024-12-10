#include "returnToken.h"

// Callback function for CURL to write data
size_t TokenManager::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    if (userp) {
        userp->append(static_cast<char*>(contents), size * nmemb);
    }
    return size * nmemb;
}

// Checks if the token has expired
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
        return true;
    }
}

// Refreshes the token using the refresh token
bool TokenManager::refreshToken() {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) {
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
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Enable SSL verification
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // Verify the host

        CURLcode res = curl_easy_perform(curl);

        bool success = (res == CURLE_OK);
        if (success) {
            json new_token = json::parse(response);
            token_data["access_token"] = new_token["access_token"];
            token_data["created_at"] = std::time(nullptr);
            token_data["expires_in"] = new_token["expires_in"];
            std::ofstream file(config->TOKEN_FILE);
            if (file.is_open()) {
                file << token_data.dump(4);
                file.close();
            }
            else {
                success = false;
            }
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return success;
    }
    catch (const std::exception& e) {
        return false;
    }
}

// Loads token data from a file
bool TokenManager::loadTokenFile() {
    try {
        std::ifstream file(config->TOKEN_FILE);
        if (!file.is_open()) {
            return false;
        }

        json token_json;
        file >> token_json;
        file.close();

        if (token_json.is_null() || token_json.empty()) {
            return false;
        }

        token_data = token_json;

        if (!token_data.contains("access_token") ||
            !token_data.contains("expires_in") ||
            !token_data.contains("refresh_token")) {
            return false;
        }

        if (!token_data.contains("created_at")) {
            token_data["created_at"] = time(nullptr);
            std::ofstream out_file(config->TOKEN_FILE);
            if (out_file.is_open()) {
                out_file << token_data.dump(4);
                out_file.close();
            }
            else {
                return false;
            }
        }

        return true;
    }
    catch (const json::parse_error& e) {
        return false;
    }
    catch (const std::exception& e) {
        return false;
    }
}

// Ensures the access token is valid
std::string TokenManager::getValidAccessToken() {
    try {
        if (!loadTokenFile()) {
            return "";
        }

        if (isTokenExpired()) {
            if (!refreshToken()) {
                return "";
            }
        }

        return token_data["access_token"].get<std::string>();
    }
    catch (const std::exception& e) {
        return "";
    }
}