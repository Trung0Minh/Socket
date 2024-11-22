// ClientSocket.cpp
#include "ClientSocket.h"
#include <iostream>

ClientSocket::ClientSocket()
    : connectSocket(INVALID_SOCKET)
    , result(nullptr)
    , ptr(nullptr)
{
    ZeroMemory(&hints, sizeof(hints));
}

ClientSocket::~ClientSocket() {
    Close();
}

bool ClientSocket::initializeWinsock() {
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
        return false;
    }
    return true;
}

bool ClientSocket::createSocket(const char* address, const char* port, int family) {
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed with error: " << iResult << std::endl;
        WSACleanup();
        return false;
    }

    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }
        break;
    }

    if (connectSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        WSACleanup();
        return false;
    }

    return true;
}

bool ClientSocket::connectToServer() {
    int iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
        freeaddrinfo(result);
        WSACleanup();
        return false;
    }

    freeaddrinfo(result);
    return true;
}

bool ClientSocket::Connect(const char* address, const char* port, int family) {
    if (!initializeWinsock()) return false;
    if (!createSocket(address, port, family)) return false;
    return connectToServer();
}

bool ClientSocket::Close() {
    if (connectSocket != INVALID_SOCKET) {
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    WSACleanup();
    return true;
}

bool ClientSocket::IsConnected() const {
    return connectSocket != INVALID_SOCKET;
}

bool ClientSocket::Send(const char* data, int length) {
    int result = send(connectSocket, data, length, 0);
    return result != SOCKET_ERROR;
}

int ClientSocket::Receive(char* buffer, int bufferSize) {
    return recv(connectSocket, buffer, bufferSize, 0);
}