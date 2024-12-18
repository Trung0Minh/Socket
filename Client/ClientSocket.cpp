#include "ClientSocket.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>

ClientSocket::ClientSocket()
    : connectSocket(INVALID_SOCKET)
    , result(nullptr)
    , ptr(nullptr)
{
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (!initializeWinsock()) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
}

ClientSocket::~ClientSocket() {
    Close();
    WSACleanup();
}

bool ClientSocket::initializeWinsock() {
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        log("WSAStartup failed");
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
    int iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        log("getaddrinfo failed");
        return false;
    }

    // Thử tạo socket với tất cả các địa chỉ có thể
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            log("Socket creation failed");
            continue;
        }
        break;
    }

    if (connectSocket == INVALID_SOCKET) {
        freeAddrInfo();
        return false;
    }

    return true;
}

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
        log("Connection failed");
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
        freeAddrInfo();
        return false;
    }

    freeAddrInfo();
    return true;
}

bool ClientSocket::Connect(const char* address, const char* port, int family) {
    return createSocket(address, port, family) && connectToServer();
}

bool ClientSocket::Close() {
    if (connectSocket != INVALID_SOCKET) {
        shutdown(connectSocket, SD_BOTH);
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    freeAddrInfo();
    return true;
}

bool ClientSocket::IsConnected() const {
    return connectSocket != INVALID_SOCKET && const_cast<ClientSocket*>(this)->checkConnection();
}

bool ClientSocket::Send(const char* data, int length) {
    if (!checkConnection()) return false;

    int totalSent = 0;
    int retryCount = 0;

    while (totalSent < length && retryCount < MAX_RETRY_COUNT) {
        int result = send(connectSocket, data + totalSent, length - totalSent, 0);
        if (result == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                Sleep(100);
                retryCount++;
                if (!checkConnection()) {
                    Close();
                    return false;
                }
                continue;
            }
            Close();
            return false;
        }
        totalSent += result;
        retryCount = 0;
    }

    return totalSent == length;
}

size_t ClientSocket::Receive(char* outputBuffer, size_t bufferSize) {
    if (!checkConnection()) return static_cast<size_t>(-1);

    std::string headerStr;
    char headerByte;
    bool foundNewline = false;

    while (!foundNewline) {
        int result = recv(connectSocket, &headerByte, 1, 0);
        if (result <= 0) {
            log("Failed to receive header");
            return static_cast<size_t>(-1);
        }
        if (headerByte == '\n') {
            foundNewline = true;
        }
        headerStr += headerByte;
    }

    size_t sizePos = headerStr.find("SIZE:");
    if (sizePos == std::string::npos) {
        log("Invalid header format: no SIZE field");
        return static_cast<size_t>(-1);
    }

    sizePos += 5;  // Skip "SIZE:"
    size_t sizeSep = headerStr.find('|', sizePos);
    if (sizeSep == std::string::npos) sizeSep = headerStr.find('\n', sizePos);

    std::string sizeStr = headerStr.substr(sizePos, sizeSep - sizePos);
    size_t dataSize;
    try {
        dataSize = std::stoull(sizeStr);
    }
    catch (...) {
        log("Invalid size format in header");
        return static_cast<size_t>(-1);
    }

    if (dataSize + headerStr.size() > bufferSize) {
        log("Received data size exceeds buffer capacity");
        return static_cast<size_t>(-1);
    }

    std::memcpy(outputBuffer, headerStr.c_str(), headerStr.size());
    size_t totalReceived = headerStr.size();

    while (totalReceived < dataSize + headerStr.size()) {
        size_t bytesToReceive = min(
            static_cast<size_t>(8192),
            dataSize + headerStr.size() - totalReceived
        );
        int bytesReceived = recv(connectSocket,
            outputBuffer + totalReceived,
            static_cast<int>(bytesToReceive),  // recv vẫn yêu cầu int
            0);

        if (bytesReceived <= 0) {
            log("Failed to receive file data");
            return static_cast<size_t>(-1);
        }
        totalReceived += bytesReceived;
    }

    std::stringstream ss;
    ss << "Data received successfully! Size: " << totalReceived << " bytes" << std::endl;
    log(ss.str());

    return totalReceived;
}

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

void ClientSocket::freeAddrInfo() {
    if (result != nullptr) {
        freeaddrinfo(result);
        result = nullptr;
        ptr = nullptr;
    }
}

void ClientSocket::log(const std::string& message) {
    if (logCallback) {
        logCallback(message);
    }
}