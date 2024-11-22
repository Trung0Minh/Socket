// ClientSocket.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

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
    std::string serverName;
    static const int DEFAULT_BUFLEN = 4096;

    bool initializeWinsock();
    bool createSocket(const char* address, const char* port, int family);
    bool connectToServer();

public:
    ClientSocket();
    ~ClientSocket();

    bool Connect(const char* address, const char* port, int family);
    bool Close();
    bool IsConnected() const;

    bool Send(const char* data, int length);
    int Receive(char* buffer, int bufferSize);

    SOCKET GetSocket() const { return connectSocket; }
};