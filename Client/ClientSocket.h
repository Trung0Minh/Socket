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
    struct addrinfo* result, * ptr, hints;
    static constexpr int SOCKET_TIMEOUT = 30000;
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr int BUFFER_CHUNK_SIZE = 8192;

    std::function<void(const std::string&)> logCallback;

    bool initializeWinsock();
    void freeAddrInfo();
    bool setNonBlocking(SOCKET sock);
    void log(const std::string& message);

public:
    ClientSocket();
    ~ClientSocket();

    void setLogCallback(std::function<void(const std::string&)> callback);
    bool Connect(const char* address, const char* port, int family);
    bool Close();
    bool IsConnected();
    bool Send(const char* data, int length);
    size_t Receive(char* outputBuffer, size_t bufferSize);
    SOCKET GetSocket() const { return connectSocket; }
};