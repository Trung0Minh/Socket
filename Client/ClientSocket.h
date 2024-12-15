#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <functional>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

class ClientSocket {
private:
    WSADATA wsaData;
    SOCKET connectSocket;
    struct addrinfo* result;
    struct addrinfo* ptr;
    struct addrinfo hints;
    static const int DEFAULT_BUFLEN = 4096;
    static const int SOCKET_TIMEOUT = 30000; // 5 seconds
    static const int MAX_RETRY_COUNT = 3;
    static const int KEEPALIVE_TIME = 10000;    // 10 seconds
    static const int KEEPALIVE_INTERVAL = 1000; // 1 second

    std::function<void(const std::string&)> logCallback;

    bool initializeWinsock();
    bool createSocket(const char* address, const char* port, int family);
    bool connectToServer();
    void freeAddrInfo();
    void log(const std::string& message);

public:
    ClientSocket();
    ~ClientSocket();

    void setLogCallback(std::function<void(const std::string&)> callback) { logCallback = callback; }
    bool Connect(const char* address, const char* port, int family);
    bool Close();
    bool IsConnected() const;
    bool Send(const char* data, int length);
    int Receive(char* outputBuffer, int bufferSize);
    SOCKET GetSocket() const { return connectSocket; }
    void setNonBlocking(bool nonBlocking);
    bool checkConnection();
};