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
    return connectToServer();
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

    // Kiểm tra xem socket có còn readable không
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(connectSocket, &readfds);

    // Timeout 0 để kiểm tra ngay lập tức
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Kiểm tra trạng thái socket
    int result = select(0, &readfds, nullptr, nullptr, &timeout);

    if (result == SOCKET_ERROR) {
        return false;
    }

    // Nếu socket readable, kiểm tra xem có dữ liệu pending không
    if (result > 0) {
        char buff;
        result = recv(connectSocket, &buff, 1, MSG_PEEK);
        // Nếu recv trả về 0, nghĩa là connection đã bị đóng
        if (result == 0) {
            return false;
        }
    }

    return true;
}

bool ClientSocket::Send(const char* data, int length) {
    if (connectSocket == INVALID_SOCKET) return false;

    int totalSent = 0;
    while (totalSent < length) {
        int result = send(connectSocket, data + totalSent, length - totalSent, 0);
        if (result == SOCKET_ERROR) {
            // Nếu gửi thất bại, đánh dấu socket là invalid
            connectSocket = INVALID_SOCKET;
            printError("Send failed");
            return false;
        }
        totalSent += result;
    }
    return true;
}

int ClientSocket::Receive(char* buffer, int bufferSize) {
    if (connectSocket == INVALID_SOCKET) return -1;

    int result = recv(connectSocket, buffer, bufferSize, 0);
    if (result == SOCKET_ERROR || result == 0) {
        // Nếu nhận thất bại hoặc connection closed, đánh dấu socket là invalid
        connectSocket = INVALID_SOCKET;
        return -1;
    }
    return result;
}