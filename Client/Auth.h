// Auth.h
#ifndef AUTH_H
#define AUTH_H

#include <string>
#include "json.hpp"

class Auth {
public:
    Auth(const std::string& credentialsFile, const std::string& tokenFile = "");
    bool authenticate();
    bool hasValidToken();
    std::string getAuthUrl() const;
    void processAuthorizationCode(const std::string& authCode);
    bool isAuthenticationCancelled() const { return authenticationCancelled; }

private:
    void requestAuthorizationCode();
    void saveToken(const nlohmann::json& token);
    bool authenticationCancelled = false;

    nlohmann::json exchangeAuthorizationCodeForTokens(const std::string& authCode);
    nlohmann::json loadCredentials();
    nlohmann::json loadToken();

    std::string credentialsFile;
    std::string tokenFile;
    nlohmann::json token;
    std::string clientId;
    std::string clientSecret;
    std::string authUri;
    std::string tokenUri;
};

#endif // AUTH_H