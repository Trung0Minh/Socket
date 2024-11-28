﻿// ClientSocket.cpp
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

// Trong hàm connectToServer()
bool ClientSocket::connectToServer() {
    if (connectSocket == INVALID_SOCKET) {
        return false;
    }

    // Set timeout trước khi connect
    DWORD timeout = SOCKET_TIMEOUT;
    setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(connectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    // Enable keep-alive
    BOOL keepAlive = TRUE;
    setsockopt(connectSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));

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
    // Nếu đã có socket cũ, đóng nó trước
    if (connectSocket != INVALID_SOCKET) {
        Close();
    }

    if (!initializeWinsock()) {
        return false;
    }
    if (!createSocket(address, port, family)) {
        return false;
    }

    bool connected = connectToServer();
    if (connected) {
        // Set socket to non-blocking mode after successful connection
        setNonBlocking(true);
    }
    return connected;
}

bool ClientSocket::Close() {
    bool success = true;
    if (connectSocket != INVALID_SOCKET) {
        shutdown(connectSocket, SD_BOTH);
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

    // Cast away const để gọi checkConnection
    return const_cast<ClientSocket*>(this)->checkConnection();
}

bool ClientSocket::Send(const char* data, int length) {
    if (!checkConnection()) return false;

    int totalSent = 0;
    int retryCount = 0;

    while (totalSent < length && retryCount < MAX_RETRY_COUNT) {
        int result = send(connectSocket, data + totalSent, length - totalSent, 0);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(100);
                retryCount++;
                if (!checkConnection()) {
                    Close();
                    return false;
                }
                continue;
            }
            printError("Send failed");
            Close();
            return false;
        }
        totalSent += result;
        retryCount = 0; // Reset retry count on successful send
    }

    return totalSent == length;
}

int ClientSocket::Receive(char* buffer, int bufferSize) {
    if (!checkConnection()) return -1;

    int totalReceived = 0;
    int retryCount = 0;

    while (retryCount < MAX_RETRY_COUNT) {
        int result = recv(connectSocket, buffer + totalReceived,
            bufferSize - totalReceived, 0);

        if (result > 0) {
            totalReceived += result;
            if (totalReceived >= bufferSize) break;
            continue;
        }

        if (result == 0) {
            // Connection closed by server
            Close();
            return totalReceived > 0 ? totalReceived : -1;
        }

        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            Sleep(100);
            retryCount++;
            if (!checkConnection()) {
                Close();
                return -1;
            }
            continue;
        }

        // Other errors indicate connection problem
        printError("Receive failed");
        Close();
        return -1;
    }

    return totalReceived;
}

void ClientSocket::setNonBlocking(bool nonBlocking) {
    if (connectSocket != INVALID_SOCKET) {
        u_long mode = nonBlocking ? 1 : 0;
        ioctlsocket(connectSocket, FIONBIO, &mode);
    }
}

// Đơn giản hóa hàm checkConnection
bool ClientSocket::checkConnection() {
    if (connectSocket == INVALID_SOCKET) {
        return false;
    }

    // Kiểm tra kết nối bằng select
    fd_set readSet, writeSet, errorSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(connectSocket, &readSet);
    FD_SET(connectSocket, &writeSet);
    FD_SET(connectSocket, &errorSet);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500ms

    // Kiểm tra cả đọc, ghi và lỗi
    int result = select(0, &readSet, &writeSet, &errorSet, &timeout);

    if (result == SOCKET_ERROR) {
        Close();
        return false;
    }

    // Kiểm tra nếu có lỗi
    if (FD_ISSET(connectSocket, &errorSet)) {
        Close();
        return false;
    }

    // Kiểm tra xem có dữ liệu đang chờ đọc không
    if (FD_ISSET(connectSocket, &readSet)) {
        char tmp[1];
        result = recv(connectSocket, tmp, 1, MSG_PEEK);
        if (result == 0 || result == SOCKET_ERROR) {
            // Connection closed or error
            Close();
            return false;
        }
    }

    return FD_ISSET(connectSocket, &writeSet);
}