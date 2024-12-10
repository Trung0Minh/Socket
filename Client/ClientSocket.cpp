#include "ClientSocket.h"
#include <iostream>
#include <vector>
#include <algorithm>

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
        logError("WSAStartup failed");
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
        logError("getaddrinfo failed");
        return false;
    }

    // Thử tạo socket với tất cả các địa chỉ có thể
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            logError("Socket creation failed");
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
        logError("Connection failed");
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
        freeAddrInfo();
        return false;
    }

    freeAddrInfo();
    return true;
}

bool ClientSocket::Connect(const char* address, const char* port, int family) {
    // Nếu đã có socket cũ, đóng nó trước
    if (connectSocket != INVALID_SOCKET) {
        Close();
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
    if (connectSocket != INVALID_SOCKET) {
        shutdown(connectSocket, SD_BOTH);
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    freeAddrInfo();
    return true;
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
            logError("Send failed");
            Close();
            return false;
        }
        totalSent += result;
        retryCount = 0; // Reset retry count on successful send
    }

    return totalSent == length;
}

int ClientSocket::Receive(char* buffer) {
    if (!checkConnection()) return -1;

    // Nhận header (giả sử header không quá 1024 bytes)
    char headerBuffer[1024];
    int bytesReceived = recv(connectSocket, headerBuffer, sizeof(headerBuffer), 0);
    if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR) {
        logError("Failed to receive header");
        return -1;
    }

    // Phân tích header để lấy kích thước dữ liệu chính
    std::string header(headerBuffer, bytesReceived);
    size_t sizePos = header.find("SIZE:");
    if (sizePos == std::string::npos) {
        logError("Invalid header format");
        return -1;
    }

    sizePos += 5;  // Skip "SIZE:"
    size_t endSizePos = header.find('\n', sizePos);
    if (endSizePos == std::string::npos) {
        logError("Invalid header format");
        return -1;
    }

    std::string sizeStr = header.substr(sizePos, endSizePos - sizePos);
    int dataSize = std::stoi(sizeStr);

    // Nhận dữ liệu chính theo từng chunk
    std::vector<char> dataBuffer(dataSize);
    int totalReceived = 0;
    char chunkBuffer[1024];

    while (totalReceived < dataSize) {
        int bytesToReceive = min(1024, dataSize - totalReceived);
        bytesReceived = recv(connectSocket, chunkBuffer, bytesToReceive, 0);
        if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR) {
            logError("Failed to receive data");
            return -1;
        }
        std::memcpy(dataBuffer.data() + totalReceived, chunkBuffer, bytesReceived);
        totalReceived += bytesReceived;
    }
    return totalReceived;
}

void ClientSocket::setNonBlocking(bool nonBlocking) {
    if (connectSocket != INVALID_SOCKET) {
        u_long mode = nonBlocking ? 1 : 0;
        ioctlsocket(connectSocket, FIONBIO, &mode);
    }
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

void ClientSocket::logError(const char* message) {
    if (logCallback) {
        logCallback(std::string(message) + " Error: " + std::to_string(WSAGetLastError()));
    }
    else {
        std::cerr << message << " Error: " << WSAGetLastError() << std::endl;
    }
}