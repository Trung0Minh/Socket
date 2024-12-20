#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma comment(lib, "gdiplus.lib")

#include <iostream>
#include <WS2tcpip.h>
#include <string>
#include <TlHelp32.h>
#include <set>
#include <windows.h>
#include <sstream>
#include <ctime>
#include <gdiplus.h>
#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <fstream>
#include "stb_image_write.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment (lib, "Ws2_32.lib")

std::string getCurrentTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    // Xây dựng chuỗi thời gian với định dạng "YYYY-MM-DD_HH-MM-SS
    oss << (tm.tm_year + 1900) << "-" // (tm.tm_year + 1900) vì tm.tm_year là số năm tính từ 1900
        << (tm.tm_mon + 1) << "-" // Tháng (0-11, nên cộng thêm 1)
        << tm.tm_mday << "_"
        << tm.tm_hour << "-"
        << tm.tm_min << "-"
        << tm.tm_sec;
    return oss.str();
}

void sendFile(SOCKET clientSocket, const std::string& filename) {
    // Mở tệp tin ở chế độ nhị phân và đặt con trỏ tệp ở cuối để lấy kích thước tệp
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::string errorMessage = "TYPE:text|SIZE:0\nFailed to open file.";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Lấy kích thước tệp
    std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    // Lấy tên tệp (bỏ đi phần đường dẫn nếu có)    
    size_t lastSlash = filename.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
    std::string mimeType;

    // Lấy phần mở rộng của tệp
    size_t lastDot = fileName.find_last_of(".");
    std::string extension = (lastDot != std::string::npos) ? fileName.substr(lastDot) : "";

    // Chuyển phần mở rộng thành chữ thường để so sánh không phân biệt chữ hoa chữ thường
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Kiểm tra phần mở rộng và xác định loại MIME tương ứng
    if (extension == ".png") {
        mimeType = "image/png";
    }
    else if (extension == ".mp4") {
        mimeType = "video/mp4";
    }
    // Add text file types
    else if (extension == ".txt") {
        mimeType = "text/plain";
    }
    else {
        std::cerr << "Unsupported file format: " << filename << std::endl;
        std::string errorMessage = "TYPE:text|SIZE:0\nUnsupported file format.";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        inFile.close();
        return;
    }

    // Gửi thông tin tiêu đề (Header) bao gồm loại tệp, kích thước tệp, tên tệp và loại MIME
    std::string header = "TYPE:file|SIZE:" + std::to_string(fileSize) + "|FILENAME:" + fileName + "|MIMETYPE:" + mimeType + "\n";
    if (send(clientSocket, header.c_str(), header.size(), 0) != static_cast<int>(header.size())) {
        std::cerr << "Failed to send header" << std::endl;
        inFile.close();
        return;
    }

    // Send file data
    char buffer[1024];
    while (inFile) {
        inFile.read(buffer, sizeof(buffer));
        int bytesRead = inFile.gcount();
        if (bytesRead > 0) {
            if (send(clientSocket, buffer, bytesRead, 0) != bytesRead) {
                std::cerr << "Failed to send file data" << std::endl;
                inFile.close();
                return;
            }
        }
    }

    std::cout << "File sent successfully!" << std::endl;
    inFile.close();
}

void sendText(SOCKET clientSocket, const std::string& message) {
    // Gửi thông tin tiêu đề (Header) bao gồm loại tệp (text), kích thước tệp, tên tệp và loại MIME
    std::string header = "TYPE:text|SIZE:" + std::to_string(message.size()) + "|FILENAME:message.txt|MIME:text/plain\n";
    int headerResult = send(clientSocket, header.c_str(), header.size(), 0);

    // Kiểm tra lỗi khi gửi tiêu đề
    if (headerResult == SOCKET_ERROR) {
        std::cerr << "Failed to send header! Error code: " << WSAGetLastError() << std::endl;
        return;
    }

    // Gửi nội dung văn bản (message)
    int messageResult = send(clientSocket, message.c_str(), message.size(), 0);

    // Kiểm tra lỗi khi gửi nội dung
    if (messageResult == SOCKET_ERROR) {
        std::cerr << "Failed to send message! Error code: " << WSAGetLastError() << std::endl;
        return;
    }

    std::cout << "Text sent successfully!" << std::endl;
}

void deleteFile(SOCKET clientSocket, const std::string& filePath) {
    // Xóa tệp bằng cách sử dụng hàm std::remove
    if (std::remove(filePath.c_str()) == 0) {
        std::string message = "File \"" + filePath + "\" has been successfully deleted.\n";
        std::cout << message << std::endl;
        sendText(clientSocket, message);
        return;
    }
    else {
        std::string error = "Unable to delete file " + filePath;
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }
}

void listApplications(SOCKET clientSocket) {
    // Khai báo set để lưu tên ứng dụng và ID tiến trình
    std::set<std::pair<std::string, DWORD>> appDetails;

    // Tạo snapshot của tất cả các tiến trình đang chạy
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::string error = "Failed to create process snapshot.";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Lấy thông tin về tiến trình đầu tiên
    if (!Process32First(hProcessSnap, &pe32)) {
        std::string error = "Failed to retrieve information about the first process.";
        std::cerr << error << std::endl;
        CloseHandle(hProcessSnap);
        return;
    }

    do {
        // Mở tiến trình để kiểm tra cửa sổ của nó
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            HWND hWnd = GetTopWindow(NULL);
            while (hWnd) {
                DWORD processID;
                GetWindowThreadProcessId(hWnd, &processID);
                // Kiểm tra nếu cửa sổ thuộc về tiến trình và cửa sổ có giao diện người dùng
                if (processID == pe32.th32ProcessID && GetWindow(hWnd, GW_OWNER) == NULL && IsWindowVisible(hWnd)) {
                    appDetails.insert({ std::string(pe32.szExeFile), pe32.th32ProcessID });
                    break; // Nếu tìm thấy cửa sổ, không cần duyệt tiếp các cửa sổ khác của tiến trình này
                }
                hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    std::string filename = "Applications_" + getCurrentTimestamp() + ".txt";
    std::ofstream outFile(filename);

    if (!outFile) {
        std::string error = "Failed to create file";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Lưu danh sách các ứng dụng với ID vào file txt
    int cnt = 1;
    for (const auto& app : appDetails) {
        outFile << std::to_string(cnt++) + ". " + app.first + " (ID: " + std::to_string(app.second) + ")\n";
    }

    outFile.close();
    sendFile(clientSocket, filename);
    std::cout << "Successfully sent list of running applications to client." << std::endl;
}

void startApplicationByName(SOCKET clientSocket, const std::string& appName) {
    // Nếu ứng dụng không có đuôi .exe, thêm vào
    std::string appNameWithExe = appName;
    if (appNameWithExe.find(".exe") == std::string::npos) {
        appNameWithExe += ".exe";
    }

    // Gọi ứng dụng trực tiếp với ShellExecuteA, hàm trả về một HINSTANCE, nếu giá trị <= 32 có nghĩa là có lỗi xảy ra
    HINSTANCE result = ShellExecuteA(NULL, "open", appNameWithExe.c_str(), NULL, NULL, SW_SHOWNORMAL);
    std::string message = (int)result <= 32 
        ? "Failed to start application \"" + appNameWithExe + "\". Error code: " + std::to_string((int)result) + '\n' 
        : "Successfully started application \"" + appNameWithExe + "\".\n";
    
    sendText(clientSocket, message);
    std::cout << message;
}

void stopAppsAndProcById(SOCKET clientSocket, DWORD processId) { //stop Applications or Process by ID
    // Mở quá trình với quyền terminate
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);

    // Kiểm tra nếu không thể mở tiến trình với quyền terminate
    if (hProcess == NULL) {
        std::string errorMessage = "Unable to open process with ID " + std::to_string(processId) + ". Error: " + std::to_string(GetLastError());
        std::cerr << errorMessage << std::endl;
        sendText(clientSocket, errorMessage);
        return;
    }

    // Đóng quá trình
    if (TerminateProcess(hProcess, 0) == 0) {
        std::string errorMessage = "Failed to terminate process with ID " + std::to_string(processId) + ". Error: " + std::to_string(GetLastError());
        std::cerr << errorMessage << std::endl;
        sendText(clientSocket, errorMessage);
        CloseHandle(hProcess);
        return;
    }

    CloseHandle(hProcess);
    std::string successMessage = "Successfully closed the application with ID " + std::to_string(processId) + ".\n";
    sendText(clientSocket, successMessage);
    std::cout << successMessage;
}

void listServices(SOCKET clientSocket) {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (hSCManager == NULL) {
        std::cerr << "Failed to open Service Control Manager: " << GetLastError() << std::endl;
        return;
    }

    DWORD bytesNeeded = 0, servicesReturned = 0, resumeHandle = 0;
    ENUM_SERVICE_STATUS_PROCESS* serviceStatus = nullptr;

    // Lần đầu gọi để tính toán bộ nhớ cần thiết
    if (!EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        NULL,  // Không truyền bộ nhớ cho lần gọi đầu tiên
        0,     // Đặt kích thước bộ nhớ là 0
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_MORE_DATA) {
            std::cerr << "Failed to enumerate services: " << error << std::endl;
            CloseServiceHandle(hSCManager);
            return;
        }

        // Cấp phát bộ nhớ đủ lớn để chứa thông tin dịch vụ
        serviceStatus = (ENUM_SERVICE_STATUS_PROCESS*)malloc(bytesNeeded);
        if (serviceStatus == NULL) {
            std::cerr << "Failed to allocate memory for service list." << std::endl;
            CloseServiceHandle(hSCManager);
            return;
        }
    }

    // Gọi lại EnumServicesStatusEx để lấy thông tin dịch vụ với bộ nhớ vừa cấp phát
    if (!EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        (LPBYTE)serviceStatus,
        bytesNeeded,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL)) {
        std::cerr << "Failed to enumerate services: " << GetLastError() << std::endl;
        free(serviceStatus);
        CloseServiceHandle(hSCManager);
        return;
    }

    std::string filename = "Services_" + getCurrentTimestamp() + ".txt";
    std::ofstream outFile(filename);
    for (DWORD i = 0; i < servicesReturned; ++i) {
        outFile << "Service Name: " << serviceStatus[i].lpServiceName << ", "
            << "Display Name: " << serviceStatus[i].lpDisplayName << ", "
            << "Status: " << (serviceStatus[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING ? "Running" : "Stopped") << "\n";
    }

    free(serviceStatus);
    CloseServiceHandle(hSCManager);
    sendFile(clientSocket, filename);
}

void startServiceByName(SOCKET clientSocket, const std::string& serviceName) {
    // Mở Service Control Manager với quyền SC_MANAGER_CONNECT
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCManager == NULL) {
        std::string errorMessage = "Failed to open Service Control Manager: " + std::to_string(GetLastError());
        std::cerr << errorMessage << std::endl;
        sendText(clientSocket, errorMessage);  // Gửi thông báo lỗi
        return;
    }

    std::string serviceNameStr(serviceName.begin(), serviceName.end());

    // Mở dịch vụ với quyền SERVICE_START
    SC_HANDLE hService = OpenService(hSCManager, serviceNameStr.c_str(), SERVICE_START);
    if (hService == NULL) {
        std::string errorMessage = "Failed to open service: " + std::to_string(GetLastError());
        std::cerr << errorMessage << std::endl;
        sendText(clientSocket, errorMessage);  // Gửi thông báo lỗi
        CloseServiceHandle(hSCManager);
        return;
    }

    // Bắt đầu dịch vụ
    if (!StartService(hService, 0, NULL)) {
        std::string errorMessage = "Failed to start service: " + std::to_string(GetLastError());
        std::cerr << errorMessage << std::endl;
        sendText(clientSocket, errorMessage);  // Gửi thông báo lỗi
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return;
    }

    // Đóng handle của dịch vụ và Service Control Manager
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    // Thông báo dịch vụ đã được khởi động thành công
    std::string successMessage = "Service " + serviceName + " started successfully.";
    sendText(clientSocket, successMessage);  // Gửi thông báo thành công
    std::cout << successMessage << std::endl;
}

void stopServiceByName(SOCKET clientSocket, const std::string& serviceName) {
    // Mở Service Control Manager
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    std::string error = "Failed to stop service";
    if (hSCManager == NULL) {
        std::cerr << "Failed to open Service Control Manager: " << GetLastError() << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Mở dịch vụ với quyền dừng (SERVICE_STOP)
    SC_HANDLE hService = OpenService(hSCManager, serviceName.c_str(), SERVICE_STOP);
    if (hService == NULL) {
        CloseServiceHandle(hSCManager);
        std::cerr << "Failed to open service: " << GetLastError() << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Lấy thông tin trạng thái dịch vụ
    SERVICE_STATUS serviceStatus;
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        std::cerr << "Failed to stop service: " << GetLastError() << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Đóng các handle
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    std::string message = "Service " + serviceName + " stopped successfully.";
    sendText(clientSocket, message);
    std::cout << message << std::endl;
}

void listProcesses(SOCKET clientSocket) {
    // Tạo ảnh chụp nhanh tất cả các tiến trình đang chạy
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::string error = "Failed to create process snapshot.";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Bắt đầu lấy danh sách tiến trình
    if (!Process32First(hProcessSnap, &pe32)) {
        std::string error = "Failed to retrieve information about the first process.";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        CloseHandle(hProcessSnap);
        return;
    }

    std::string filename = "Processes_" + getCurrentTimestamp() + ".txt";
    std::ofstream outFile(filename);

    if (!outFile) {
        std::string error = "Failed to create file!";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Lưu kết quả vào một file để gửi qua socket
    int count = 1;

    do {
        outFile << std::to_string(count++) + ". Process Name: " << std::string(pe32.szExeFile) <<
            ", PID: " << std::to_string(pe32.th32ProcessID) << "\n";
    } while (Process32Next(hProcessSnap, &pe32));
    outFile.close();


    // Gửi kết quả về client
    sendFile(clientSocket, filename);
    CloseHandle(hProcessSnap);

    std::cout << "Successfully send list of services to client." << std::endl;
}

void listIPs(SOCKET clientSocket) {
    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufferSize);

    IP_ADAPTER_ADDRESSES* adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);

    std::string filename = "IP_" + getCurrentTimestamp() + ".txt";
    std::ofstream outFile(filename);

    if (!outFile) {
        std::string error = "Failed to create file!";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapterAddresses, &bufferSize) == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* adapter = adapterAddresses; adapter != nullptr; adapter = adapter->Next) {
            // In tên adapter
            outFile << "Adapter: " << adapter->FriendlyName << std::endl;

            bool foundIPv4 = false;
            bool foundIPv6 = false;

            for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                char ipAddress[INET6_ADDRSTRLEN] = { 0 };

                // Kiểm tra địa chỉ IPv4
                if (unicast->Address.lpSockaddr->sa_family == AF_INET) { // IPv4
                    sockaddr_in* sockaddr = (sockaddr_in*)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sockaddr->sin_addr), ipAddress, INET6_ADDRSTRLEN);

                    // Loại bỏ IP APIPA (169.254.x.x)
                    if (strncmp(ipAddress, "169.254.", 8) != 0) {
                        if (!foundIPv4) {
                            outFile << "  IPv4 Address: " << ipAddress << std::endl;
                            foundIPv4 = true;
                        }
                    }
                }
                // Kiểm tra địa chỉ IPv6
                else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) { // IPv6
                    sockaddr_in6* sockaddr = (sockaddr_in6*)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET6, &(sockaddr->sin6_addr), ipAddress, INET6_ADDRSTRLEN);

                    if (!foundIPv6) {
                        outFile << "  IPv6 Address: " << ipAddress << std::endl;
                        foundIPv6 = true;
                    }
                }
            }
            outFile << std::endl; // Thêm dòng trống giữa các adapter
        }
        std::cout << "IP list saved to file: " << filename << std::endl;
    }
    else {
        std::string error = "Error retrieving adapter addresses";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
    }

    free(adapterAddresses);
    outFile.close();

    // Gửi tệp đến client
    sendFile(clientSocket, filename);
    std::cout << "Successfully sent list of IP to client." << std::endl;
}

void shutdownComputer(SOCKET clientSocket) {
    // Gửi thông báo cho client trước khi tắt máy
    std::string shutdownMessage = "The computer is shutting down after 10 seconds.\n";
    sendText(clientSocket, shutdownMessage);

    // Đợi một chút để client có thể nhận được thông báo
    Sleep(10000);  // Đợi 10 giây (thời gian này có thể thay đổi tùy vào tình huống)

    // Kiểm tra quyền tắt máy
    HANDLE hToken;
    std::string errorMessage = "Error shutting down the computer.\n";
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "Error opening process token. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        return;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    // Lấy giá trị LUID cho quyền tắt máy
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid)) {
        std::cerr << "Error looking up privilege value. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        CloseHandle(hToken);
        return;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid; // Gán giá trị LUID vào quyền
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) == 0) {
        std::cerr << "Error adjusting token privileges. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        CloseHandle(hToken);
        return;
    }

    CloseHandle(hToken);

    // Thực hiện tắt máy
    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_SOFTWARE | SHTDN_REASON_FLAG_PLANNED)) {
        std::cerr << "Error shutting down the computer. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
    }
    else {
        // Sau khi tắt máy thành công, không gửi thêm thông báo nữa vì hệ thống sẽ tắt
        std::cout << "Shutting down the computer..." << std::endl;
    }
}

void restartComputer(SOCKET clientSocket) {
    // Gửi thông báo cho client trước khi khởi động lại máy
    std::string restartMessage = "The computer is restarting shortly.\n";
    sendText(clientSocket, restartMessage);

    // Đợi một chút để client có thể nhận được thông báo
    Sleep(5000);  // Đợi 5 giây (thời gian này có thể thay đổi tùy vào tình huống)

    // Kiểm tra quyền tắt máy (quyền này cũng cần để khởi động lại máy)
    HANDLE hToken;
    std::string errorMessage = "Error restarting the computer.\n";
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "Error opening process token. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        return;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    // Lấy giá trị LUID cho quyền khởi động lại máy
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid)) {
        std::cerr << "Error looking up privilege value. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        CloseHandle(hToken);
        return;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid; // Gán giá trị LUID vào quyền
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) == 0) {
        std::cerr << "Error adjusting token privileges. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
        CloseHandle(hToken);
        return;
    }

    CloseHandle(hToken);

    // Thực hiện khởi động lại máy
    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_SOFTWARE | SHTDN_REASON_FLAG_PLANNED)) {
        std::cerr << "Error restarting the computer. Error: " << GetLastError() << std::endl;
        sendText(clientSocket, errorMessage);
    }
    else {
        // Sau khi khởi động lại thành công, không gửi thêm thông báo nữa vì hệ thống sẽ khởi động lại
        std::cout << "Restarting the computer..." << std::endl;
    }
}

void captureScreenshot(SOCKET clientSocket) {
    // Lấy thông tin màn hình
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);

    // Lấy kích thước màn hình
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // Tạo bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMemDC, hBitmap);

    // Chụp màn hình
    BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

    // Lấy thông tin về bitmap
    BITMAP bitmap;
    GetObject(hBitmap, sizeof(bitmap), &bitmap);

    // Lấy dữ liệu pixel từ bitmap
    int size = bitmap.bmWidth * bitmap.bmHeight * 4; // 4 bytes per pixel (RGBA)
    unsigned char* pixels = new unsigned char[size];
    GetBitmapBits(hBitmap, size, pixels);

    // Chuyển đổi từ BGRA sang RGBA
for (int i = 0; i < size; i += 4) {
    unsigned char temp = pixels[i];       // B
    pixels[i] = pixels[i + 2];            // G
    pixels[i + 2] = temp;                 // B
}

    // Lưu ảnh dưới định dạng PNG
    std::string filename = "Screenshot_" + getCurrentTimestamp() + ".png";
    stbi_write_png(filename.c_str(), bitmap.bmWidth, bitmap.bmHeight, 4, pixels, bitmap.bmWidth * 4);

    // Dọn dẹp
    delete[] pixels;
    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(NULL, hdcScreen);
    sendFile(clientSocket, filename);
}

void recordVideoFromCamera(SOCKET clientSocket, int duration_seconds) {
    cv::VideoCapture camera(0); // Mở camera mặc định (camera ID là 0)
    if (!camera.isOpened()) {
        std::string error = "Could not open the camera.";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    // Lấy FPS thực tế từ camera
    double actualFPS = camera.get(cv::CAP_PROP_FPS);
    if (actualFPS <= 0) {
        actualFPS = 30; // Giá trị mặc định nếu camera không cung cấp thông tin FPS
    }

    std::string filename = "Record_" + getCurrentTimestamp() + ".mp4";

    int frameWidth = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::Size frameSize(frameWidth, frameHeight);

    // Tạo đối tượng VideoWriter
    cv::VideoWriter videoWriter(filename, cv::VideoWriter::fourcc('H', '2', '6', '4'), actualFPS, frameSize);

    if (!videoWriter.isOpened()) {
        std::string error = "Could not open the video writer.";
        std::cerr << error << std::endl;
        sendText(clientSocket, error);
        return;
    }

    std::cout << "Start recording video..." << std::endl;

    int frameCount = static_cast<int>(duration_seconds * actualFPS); // Tính số khung hình cần ghi
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < frameCount; ++i) {
        cv::Mat frame;
        camera >> frame; // Đọc khung hình từ camera
        if (frame.empty()) {
            std::string error = "Failed to capture frame.";
            std::cerr << error << std::endl;
            sendText(clientSocket, error);
            break;
        }

        videoWriter.write(frame); // Ghi khung hình vào file video
        cv::imshow("Recording...", frame);

        // Kiểm soát thời gian giữa các khung hình
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = currentTime - startTime;
        int delay = static_cast<int>((1000.0 / actualFPS) - elapsed.count());
        if (delay > 0) {
            cv::waitKey(delay);
        }
        startTime = std::chrono::high_resolution_clock::now();
    }

    camera.release();
    videoWriter.release();
    cv::destroyAllWindows();
    std::cout << "Video recording completed and saved at " << filename << std::endl;
    sendFile(clientSocket, filename);
    std::cout << "Successfully record the video and send to client." << std::endl;
}

int main() {
    // Initialize winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);
    int wsOk = WSAStartup(ver, &wsData);
    if (wsOk != 0) {
        std::cerr << "Can't Initialize winsock! Quitting" << std::endl;
        return 1;
    }

    // Create a socket
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        std::cerr << "Can't create a socket! Quitting" << std::endl;
        return 1;
    }

    // Bind the IP address and port to the socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54000);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(listening, (sockaddr*)&hint, sizeof(hint));

    // Tell Winsock the socket is for listening
    listen(listening, SOMAXCONN);

    // Wait for a connection
    sockaddr_in client;
    int clientSize = sizeof(client);
    SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);

    char host[NI_MAXHOST];  // Client's remote name
    char service[NI_MAXSERV]; // Service (i.e. port) the client is connected on

    ZeroMemory(host, NI_MAXHOST);
    ZeroMemory(service, NI_MAXSERV);

    if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
        std::cout << host << " connected on port " << service << std::endl;
    }
    else {
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        std::cout << host << " connected on port " << ntohs(client.sin_port) << std::endl;
    }

    // Close the listening socket
    closesocket(listening);

    // While loop: accept and echo message back to client
    char buf[4096];
    while (true) {
        ZeroMemory(buf, 4096); // Reset the buffer before receiving new data
        // Wait for client to send data
        int bytesReceived = recv(clientSocket, buf, 4096, 0); // Receive data from client
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Error in recv(). Quitting" << std::endl;
            break;
        }

        if (bytesReceived == 0) {
            std::cout << "Client disconnected " << std::endl;
            break;
        }

        std::cout << std::string(buf, 0, bytesReceived) << std::endl;

        if (std::string(buf, 0, bytesReceived) == "list applications") {
            listApplications(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 9) == "startApps") {
            // Lấy tên ứng dụng từ lệnh
            std::string appName = std::string(buf, 10, bytesReceived - 10); // Lấy chuỗi sau "startApps "
            startApplicationByName(clientSocket, appName);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 8) == "stopApps") {
            // Tách PID từ chuỗi lệnh nhận được
            std::string pidStr = std::string(buf, 9, bytesReceived - 9); // Lấy chuỗi sau "stopApps "
            DWORD pidToClose = std::stoul(pidStr); // Chuyển đổi PID sang số nguyên
            stopAppsAndProcById(clientSocket, pidToClose);
        }
        else if (std::string(buf, 0, bytesReceived) == "list services") {
            listServices(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 9) == "startServ") {
            // Lấy tên ứng dụng từ lệnh
            std::string appName = std::string(buf, 10, bytesReceived - 10); // Lấy chuỗi sau "startServ "
            startServiceByName(clientSocket, appName);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 8) == "stopServ") {
            // Lấy tên ứng dụng từ lệnh
            std::string appName = std::string(buf, 9, bytesReceived - 9); // Lấy chuỗi sau "stopServ "
            stopServiceByName(clientSocket, appName);
        }
        else if (std::string(buf, 0, bytesReceived) == "list processes") {
            listProcesses(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 8) == "stopProc") {
            // Tách PID từ chuỗi lệnh nhận được
            std::string pidStr = std::string(buf, 9, bytesReceived - 9); // Lấy chuỗi sau "stopProc "
            DWORD pidToClose = std::stoul(pidStr); // Chuyển đổi PID sang số nguyên
            stopAppsAndProcById(clientSocket, pidToClose);
        }
        else if (std::string(buf, 0, bytesReceived) == "list ip") {
            listIPs(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "shutdown") {
            shutdownComputer(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "restart") {
            restartComputer(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "screenshot") {
            captureScreenshot(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 6) == "record") {
            std::string duration_seconds_str = std::string(buf, 7, bytesReceived - 7); // Lấy chuỗi sau "record "
            DWORD duration__seconds = std::stoul(duration_seconds_str); // Chuyển đổi duration_second_str sang số nguyên
            recordVideoFromCamera(clientSocket, duration__seconds);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 8) == "sendfile") {
            std::string filename_str = std::string(buf, 9, bytesReceived - 9);

            // Xóa khoảng trắng đầu và cuối
            filename_str.erase(filename_str.begin(), std::find_if(filename_str.begin(), filename_str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            filename_str.erase(std::find_if(filename_str.rbegin(), filename_str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), filename_str.end());
            sendFile(clientSocket, filename_str);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 6) == "delete") {
            std::string filename_str = std::string(buf, 7, bytesReceived - 7);

            // Xóa khoảng trắng đầu và cuối
            filename_str.erase(filename_str.begin(), std::find_if(filename_str.begin(), filename_str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            filename_str.erase(std::find_if(filename_str.rbegin(), filename_str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), filename_str.end());

            if (!filename_str.empty()) {
                deleteFile(clientSocket, filename_str);
            }
            else {
                std::string error = "Tên file không hợp lệ.";
                std::cerr << error << std::endl;
                sendText(clientSocket, error);
            }
        }
        else if (std::string(buf, 0, bytesReceived) == "help") {
            sendFile(clientSocket, "help.txt");
        }
        else if (std::string(buf, 0, bytesReceived) == "about") {
            sendFile(clientSocket, "about.txt");
        }
    }

        // Close the client socket
        closesocket(clientSocket);

        // Cleanup Winsock
        WSACleanup();

        system("pause");
        return 0;
}
