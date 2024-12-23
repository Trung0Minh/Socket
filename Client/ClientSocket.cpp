#include "ClientSocket.h"
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

void ClientSocket::setLogCallback(std::function<void(const std::string&)> callback) {
    logCallback = callback;
}

bool ClientSocket::initializeWinsock() {
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        log("WSAStartup failed");
        return false;
    }
    return true;
}

void ClientSocket::freeAddrInfo() {
    if (result) {
        freeaddrinfo(result);
        result = ptr = nullptr;
    }
}

void ClientSocket::log(const std::string& message) {
    if (logCallback) logCallback(message);
}

bool ClientSocket::setNonBlocking(SOCKET sock) {
    u_long mode = 1;  // 1 for non-blocking, 0 for blocking
    return (ioctlsocket(sock, FIONBIO, &mode) == 0);
}

bool ClientSocket::Connect(const char* address, const char* port, int family) {
    if (IsConnected()) Close();

    hints.ai_family = family;
    if (getaddrinfo(address, port, &hints, &result) != 0) {
        log("getaddrinfo failed");
        return false;
    }

    for (ptr = result; ptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket != INVALID_SOCKET) break;
    }

    if (connectSocket == INVALID_SOCKET) {
        freeAddrInfo();
        return false;
    }

    // Set socket options
    DWORD timeout = SOCKET_TIMEOUT;
    setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(connectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
    BOOL keepAlive = TRUE;
    setsockopt(connectSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));

    // Set non-blocking mode
    if (!setNonBlocking(connectSocket)) {
        Close();
        return false;
    }

    // Attempt to connect
    int result = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (result == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            Close();
            return false;
        }

        // Wait for connection with timeout
        fd_set writeSet, errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(connectSocket, &writeSet);
        FD_SET(connectSocket, &errorSet);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        result = select(0, nullptr, &writeSet, &errorSet, &tv);

        if (result == 0) {  // Timeout
            Close();
            return false;
        }
        if (result == SOCKET_ERROR) {
            Close();
            return false;
        }
        if (FD_ISSET(connectSocket, &errorSet)) {
            Close();
            return false;
        }
        if (!FD_ISSET(connectSocket, &writeSet)) {
            Close();
            return false;
        }

        // Check if connection was successful
        int error;
        int len = sizeof(error);
        if (getsockopt(connectSocket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) != 0 || error != 0) {
            Close();
            return false;
        }
    }

    // Set back to blocking mode
    u_long mode = 0;
    if (ioctlsocket(connectSocket, FIONBIO, &mode) != 0) {
        Close();
        return false;
    }

    freeAddrInfo();
    return true;
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

bool ClientSocket::IsConnected() {
    if (connectSocket == INVALID_SOCKET) return false;

    fd_set readSet, writeSet, errorSet;
    FD_ZERO(&readSet); FD_ZERO(&writeSet); FD_ZERO(&errorSet);
    FD_SET(connectSocket, &readSet);
    FD_SET(connectSocket, &writeSet);
    FD_SET(connectSocket, &errorSet);

    timeval timeout{ 0, 500000 }; // 500ms

    if (select(0, &readSet, &writeSet, &errorSet, &timeout) == SOCKET_ERROR ||
        FD_ISSET(connectSocket, &errorSet)) {
        Close();
        return false;
    }

    if (FD_ISSET(connectSocket, &readSet)) {
        char tmp;
        if (recv(connectSocket, &tmp, 1, MSG_PEEK) <= 0) {
            Close();
            return false;
        }
    }

    return FD_ISSET(connectSocket, &writeSet);
}

bool ClientSocket::Send(const char* data, int length) {
    if (!IsConnected()) return false;

    int totalSent = 0, retryCount = 0;
    while (totalSent < length && retryCount < MAX_RETRY_COUNT) {
        int result = send(connectSocket, data + totalSent, length - totalSent, 0);
        if (result == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                Sleep(100);
                if (!IsConnected()) return false;
                retryCount++;
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
    if (!IsConnected()) return static_cast<size_t>(-1);

    std::string headerStr;
    char headerByte;
    bool foundNewline = false;

    // Nhận header
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

    // Parse size từ header
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

    // Kiểm tra xem buffer có đủ lớn không
    if (dataSize + headerStr.size() > bufferSize) {
        log("Received data size exceeds buffer capacity");
        return static_cast<size_t>(-1);
    }

    // Copy header vào buffer
    std::memcpy(outputBuffer, headerStr.c_str(), headerStr.size());
    size_t totalReceived = headerStr.size();

    // Nhận phần data
    size_t dataReceived = 0;
    while (dataReceived < dataSize) {
        size_t bytesToReceive = min(
            static_cast<size_t>(8192),
            dataSize - dataReceived
        );
        int bytesReceived = recv(connectSocket,
            outputBuffer + totalReceived + dataReceived,
            static_cast<int>(bytesToReceive),
            0);

        if (bytesReceived <= 0) {
            log("Failed to receive file data");
            return static_cast<size_t>(-1);
        }
        dataReceived += bytesReceived;
    }

    std::stringstream ss;
    ss << "Data received successfully! Size: " << (totalReceived + dataReceived) << " bytes" << std::endl;
    log(ss.str());

    return totalReceived + dataReceived;
}