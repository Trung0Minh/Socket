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
        printError("WSAStartup failed");
        return false;
    }
    return true;
}

bool ClientSocket::createSocket(const char* address, const char* port, int family) {
    // Đảm bảo cleanup trước khi tạo socket mới
    if (IsConnected()) {
        Close();
    }

    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        printError("getaddrinfo failed");
        WSACleanup();
        return false;
    }

    // Thử tạo socket với tất cả các địa chỉ có thể
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            printError("Socket creation failed");
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
    if (connectSocket == INVALID_SOCKET) {
        return false;
    }

    // Thiết lập timeout cho socket
    DWORD timeout = 5000; // 5 giây
    setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(connectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    int iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printError("Connection failed");
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    return true;
}

bool ClientSocket::Connect(const char* address, const char* port, int family) {
    if (!initializeWinsock()) {
        return false;
    }
    if (!createSocket(address, port, family)) {
        return false;
    }
    return connectToServer();
}

bool ClientSocket::Close() {
    bool success = true;
    if (connectSocket != INVALID_SOCKET) {
        // Shutdown the connection for both sending and receiving
        if (shutdown(connectSocket, SD_BOTH) == SOCKET_ERROR) {
            printError("Shutdown failed");
            success = false;
        }
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    WSACleanup();
    return success;
}

bool ClientSocket::IsConnected() const {
    if (connectSocket == INVALID_SOCKET) {
        return false;
    }

    // Kiểm tra trạng thái kết nối
    char error_code;
    int error_code_size = sizeof(error_code);
    int result = getsockopt(connectSocket, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);

    return (result == 0 && error_code == 0);
}

bool ClientSocket::Send(const char* data, int length) {
    if (!IsConnected()) return false;

    int totalSent = 0;
    while (totalSent < length) {
        int result = send(connectSocket, data + totalSent, length - totalSent, 0);
        if (result == SOCKET_ERROR) {
            printError("Send failed");
            return false;
        }
        totalSent += result;
    }
    return true;
}

int ClientSocket::Receive(char* buffer, int bufferSize) {
    if (!IsConnected()) return -1;

    return recv(connectSocket, buffer, bufferSize, 0);
}