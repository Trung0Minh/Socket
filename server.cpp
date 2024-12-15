#define _CRT_NO_WARNINGS
#pragma comment(lib, "gdiplus.lib")

#include <iostream>
#include <WS2tcpip.h>
#include <string>
#include <Windows.h>
#include <TlHelp32.h>
#include <set>
#include <mfapi.h>
#include <mfplay.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <atlstr.h>
#include <windows.h>
#include <string>
#include <sstream>
#include <ctime>
#include <gdiplus.h>
#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <psapi.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <fstream>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment (lib, "Ws2_32.lib")

std::wstring convertToWString(const CHAR* str) {
    size_t len = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[len];
    size_t outSize;
    mbstowcs_s(&outSize, wstr, len, str, len - 1);
    std::wstring wresult(wstr);
    delete[] wstr;
    return wresult;
}

void listRunningApplications(SOCKET clientSocket) {
    std::set<std::pair<std::wstring, DWORD>> appDetails;

    // Tạo snapshot của tất cả các tiến trình đang chạy
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::string result = "Failed to create process snapshot.";
        send(clientSocket, result.c_str(), result.length(), 0);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Lấy thông tin về tiến trình đầu tiên
    if (!Process32First(hProcessSnap, &pe32)) {
        std::string result = "Failed to retrieve information about the first process.";
        send(clientSocket, result.c_str(), result.length(), 0);
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
                    appDetails.insert({ convertToWString(pe32.szExeFile), pe32.th32ProcessID });
                    break; // Nếu tìm thấy cửa sổ, không cần duyệt tiếp các cửa sổ khác của tiến trình này
                }
                hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    // In ra danh sách các ứng dụng với ID
    std::string result = "List of running applications:\n";
    int cnt = 1;
    for (const auto& app : appDetails) {
        result += std::to_string(cnt++) + ". " + std::string(app.first.begin(), app.first.end()) + " (ID: " + std::to_string(app.second) + ")\n";
    }
    send(clientSocket, result.c_str(), result.length() - 1, 0);
}

// pop up cua so ung dung da mo
void openApplicationById(DWORD processId) {
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        std::cerr << "Failed to retrieve information about the first process." << std::endl;
        CloseHandle(hProcessSnap);
        return;
    }

    do {
        if (pe32.th32ProcessID == processId) {
            // Mở ứng dụng
            std::wstring appPath = convertToWString(pe32.szExeFile);
            ShellExecuteW(NULL, L"open", appPath.c_str(), NULL, NULL, SW_SHOWNORMAL); // Sử dụng ShellExecuteW
            CloseHandle(hProcessSnap);
            return;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    std::cout << "Application with ID " << processId << " not found." << std::endl;
    CloseHandle(hProcessSnap);
}

void startApplicationByName(const std::wstring& appName, SOCKET clientSocket) {
    // Nếu ứng dụng không có đuôi .exe, thêm vào
    std::wstring appNameWithExe = appName;
    if (appNameWithExe.find(L".exe") == std::wstring::npos) {
        appNameWithExe += L".exe";
    }

    // Gọi ứng dụng trực tiếp với ShellExecuteW
    HINSTANCE result = ShellExecuteW(NULL, L"open", appNameWithExe.c_str(), NULL, NULL, SW_SHOWNORMAL);
    std::string result1 = (int)result <= 32 ? "Failed to start application \"" + std::string(appNameWithExe.begin(), appNameWithExe.end()) + "\". Error code: " + std::to_string((int)result) + '\n' : "Successfully started application \"" + std::string(appNameWithExe.begin(), appNameWithExe.end()) + "\".\n";
    send(clientSocket, result1.c_str(), result1.length() - 1, 0);
}

void closeApplicationById(DWORD processId, SOCKET clientSocket) {
    // Mở quá trình với quyền terminate
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == NULL) {
        std::string errorMessage = "Unable to open process with ID " + std::to_string(processId) + ". Error: " + std::to_string(GetLastError()) + '\n';
        send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
        std::cerr << errorMessage;
        return;
    }

    // Đóng quá trình
    if (TerminateProcess(hProcess, 0) == 0) {
        std::string errorMessage = "Failed to terminate process with ID " + std::to_string(processId) + ". Error: " + std::to_string(GetLastError()) + '\n';
        send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
        std::cerr << errorMessage;
        CloseHandle(hProcess);
        return;
    }

    CloseHandle(hProcess);
    std::string successMessage = "Successfully closed the application with ID " + std::to_string(processId) + ".\n";
    send(clientSocket, successMessage.c_str(), successMessage.length() - 1, 0);
    std::cout << successMessage;
}

void shutdownComputer() {
    // Kiểm tra quyền tắt máy
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "Error opening process token. Error: " << GetLastError() << std::endl;
        return;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    // Lấy giá trị LUID cho quyền tắt máy
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid)) {
        std::cerr << "Error looking up privilege value. Error: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid; // Gán giá trị LUID vào quyền
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) == 0) {
        std::cerr << "Error adjusting token privileges. Error: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return;
    }

    CloseHandle(hToken);

    // Thực hiện tắt máy
    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_SOFTWARE | SHTDN_REASON_FLAG_PLANNED)) {
        std::cerr << "Error shutting down the computer. Error: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "Shutting down the computer..." << std::endl;
    }
}

void sendFile(SOCKET clientSocket, const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::string errorMessage = "TYPE:text|SIZE:0\nFailed to open file.";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Get file size
    std::streamsize fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    // Extract filename and determine MIME type
    size_t lastSlash = filename.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
    std::string mimeType;
    if (fileName.size() >= 4 && fileName.compare(fileName.size() - 4, 4, ".png") == 0) {
        mimeType = "image/png";
    }
    else if (fileName.size() >= 4 && fileName.compare(fileName.size() - 4, 4, ".mp4") == 0) {
        mimeType = "video/mp4";
    }
    else {
        std::cerr << "Unsupported file format: " << filename << std::endl;
        std::string errorMessage = "TYPE:text|SIZE:0\nUnsupported file format.";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        inFile.close();
        return;
    }

    // Send header
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

bool deleteFile(const std::string& filePath) {
    if (std::remove(filePath.c_str()) == 0) {
        std::cout << "File \"" << filePath << "\" đã được xóa thành công.\n";
        return true;
    }
    else {
        std::cerr << "Không thể xóa file \"" << filePath << "\". Vui lòng kiểm tra lại.\n";
        return false;
    }
}

std::string getCurrentTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << (tm.tm_year + 1900) << "-"
        << (tm.tm_mon + 1) << "-"
        << tm.tm_mday << "_"
        << tm.tm_hour << "-"
        << tm.tm_min << "-"
        << tm.tm_sec;
    return oss.str();
}

void recordVideoFromCamera(SOCKET clientSocket, int duration_seconds) {
    cv::VideoCapture camera(0); // Mở camera mặc định (camera ID là 0)
    if (!camera.isOpened()) {
        std::cerr << "Could not open the camera." << std::endl;
        return;
    }

    std::string filename = "Record_" + getCurrentTimestamp() + ".mp4";
    int frameWidth = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::Size frameSize(frameWidth, frameHeight);

    // Tạo đối tượng VideoWriter để lưu video tại filename
    cv::VideoWriter videoWriter(filename, cv::VideoWriter::fourcc('H', '2', '6', '4'), 30, frameSize);

    if (!videoWriter.isOpened()) {
        std::cerr << "Could not open the video writer." << std::endl;
        return;
    }

    std::cout << "Recording video..." << std::endl;

    int frameCount = duration_seconds * 30; // 30 FPS (số khung hình mỗi giây)

    for (int i = 0; i < frameCount; ++i) {
        cv::Mat frame;
        camera >> frame; // Đọc khung hình từ camera
        if (frame.empty()) {
            std::cerr << "Failed to capture frame." << std::endl;
            break;
        }

        videoWriter.write(frame); // Ghi khung hình vào file video
        cv::imshow("Recording...", frame);

        if (cv::waitKey(33) == 27) { // Nhấn 'Esc' để dừng sớm
            break;
        }
    }

    camera.release();
    videoWriter.release();
    cv::destroyAllWindows();
    std::cout << "Video recording completed and saved at " << filename << std::endl;
    sendFile(clientSocket, filename);
}
void listAllProcesses(SOCKET clientSocket) {
    // Tạo ảnh chụp nhanh tất cả các tiến trình đang chạy
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::string result = "Failed to create process snapshot.";
        send(clientSocket, result.c_str(), result.length(), 0);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Bắt đầu lấy danh sách tiến trình
    if (!Process32First(hProcessSnap, &pe32)) {
        std::string result = "Failed to retrieve information about the first process.";
        send(clientSocket, result.c_str(), result.length(), 0);
        CloseHandle(hProcessSnap);
        return;
    }

    // Lưu kết quả vào một chuỗi để gửi qua socket
    std::string processList = "List of all running processes:\n";
    int count = 1;

    do {
        processList += std::to_string(count++) + ". Process Name: " + std::string(pe32.szExeFile) +
            ", PID: " + std::to_string(pe32.th32ProcessID) + "\n";
    } while (Process32Next(hProcessSnap, &pe32));

    // Gửi kết quả về client
    send(clientSocket, processList.c_str(), processList.length() - 1, 0);

    CloseHandle(hProcessSnap);
}

void saveIPListToFile(const std::string& filename) {
    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_INET, 0, nullptr, nullptr, &bufferSize);

    IP_ADAPTER_ADDRESSES* adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
    std::ofstream outFile(filename);

    if (!outFile) {
        std::cerr << "Failed to create file!" << std::endl;
        return;
    }

    if (GetAdaptersAddresses(AF_INET, 0, nullptr, adapterAddresses, &bufferSize) == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* adapter = adapterAddresses; adapter != nullptr; adapter = adapter->Next) {
            for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                char ipAddress[INET_ADDRSTRLEN];
                sockaddr_in* sockaddr = (sockaddr_in*)unicast->Address.lpSockaddr;
                inet_ntop(AF_INET, &(sockaddr->sin_addr), ipAddress, INET_ADDRSTRLEN);
                outFile << ipAddress << std::endl; // Ghi IP vào tệp
            }
        }
        std::cout << "IP list saved to file: " << filename << std::endl;
    }
    else {
        std::cerr << "Error retrieving adapter addresses!" << std::endl;
    }

    free(adapterAddresses);
    outFile.close();
}

void receiveFile(SOCKET serverSocket, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return;
    }

    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(serverSocket, buffer, sizeof(buffer), 0)) > 0) {
        outFile.write(buffer, bytesReceived);
    }

    std::cout << "File received and saved to: " << filename << std::endl;
    outFile.close();
}

void sendText(SOCKET clientSocket, const std::string& message) {
    // Send header
    std::string header = "TYPE:text|SIZE:" + std::to_string(message.size()) + "\n";
    send(clientSocket, header.c_str(), header.size(), 0);

    // Send text content
    send(clientSocket, message.c_str(), message.size(), 0);

    std::cout << "Text sent successfully!" << std::endl;
}

void takeScreenshotWithTimestamp(SOCKET clientSocket) {
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create a device context compatible with the screen
    HDC screenDC = GetDC(NULL);
    HDC memoryDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, screenWidth, screenHeight);
    SelectObject(memoryDC, hBitmap);

    // Copy the screen content into the memory device context
    BitBlt(memoryDC, 0, 0, screenWidth, screenHeight, screenDC, 0, 0, SRCCOPY | CAPTUREBLT);

    
    std::string filename = "Screenshot_" + getCurrentTimestamp() + ".png";

    // Save the bitmap as a PNG file
    Gdiplus::Bitmap bitmap(hBitmap, NULL);
    CLSID pngClsid;
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);
    std::wstring wideFilename(filename.begin(), filename.end());
    bitmap.Save(wideFilename.c_str(), &pngClsid, NULL);

    // Clean up
    DeleteObject(hBitmap);
    DeleteDC(memoryDC);
    ReleaseDC(NULL, screenDC);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    std::cout << "Screenshot saved as " << filename << std::endl;
    sendFile(clientSocket, filename);
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
    char buf[102400];
    while (true) {
        ZeroMemory(buf, 102400);
        // Wait for client to send data
        int bytesReceived = recv(clientSocket, buf, 102400, 0);
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Error in recv(). Quitting" << std::endl;
            break;
        }

        if (bytesReceived == 0) {
            std::cout << "Client disconnected " << std::endl;
            break;
        }

        std::cout << std::string(buf, 0, bytesReceived) << std::endl;

        if (std::string(buf, 0, bytesReceived) == "list") {
            listRunningApplications(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 6) == "remove") {
            // Tách PID từ chuỗi lệnh nhận được
            std::string pidStr = std::string(buf, 7, bytesReceived - 7); // Lấy chuỗi sau "remove "
            DWORD pidToClose = std::stoul(pidStr); // Chuyển đổi PID sang số nguyên
            closeApplicationById(pidToClose, clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "shutdown") {
            shutdownComputer();
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 6) == "camera") {
            std::string duration_seconds_str = std::string(buf, 7, bytesReceived - 7); // Lấy chuỗi sau "camera"
            DWORD duration__seconds = std::stoul(duration_seconds_str); // Chuyển đổi duration_second_str sang số nguyên
            recordVideoFromCamera(clientSocket, duration__seconds);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 5) == "start") {
            // Lấy tên ứng dụng từ lệnh
            std::string appNameStr = std::string(buf, 6, bytesReceived - 6); // Lấy chuỗi sau "start "
            std::wstring appName = convertToWString(appNameStr.c_str());    // Chuyển sang wstring

            // Gọi hàm để khởi chạy ứng dụng
            startApplicationByName(appName, clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "process") {
            listAllProcesses(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived) == "send") {
            saveIPListToFile("ip.txt");
            sendFile(clientSocket, "ip.txt");
        }
        else if (std::string(buf, 0, bytesReceived) == "pic") {
            takeScreenshotWithTimestamp(clientSocket);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 7) == "sendfile") {
            std::string filename_str = std::string(buf, 8, bytesReceived - 8);
            // Xóa khoảng trắng đầu và cuối
            filename_str.erase(filename_str.begin(), std::find_if(filename_str.begin(), filename_str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            filename_str.erase(std::find_if(filename_str.rbegin(), filename_str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), filename_str.end());
            sendFile(clientSocket, filename_str);
        }
        else if (std::string(buf, 0, bytesReceived).substr(0, 5) == "delete") {
            std::string filename_str = std::string(buf, 6, bytesReceived - 6);

            // Xóa khoảng trắng đầu và cuối
            filename_str.erase(filename_str.begin(), std::find_if(filename_str.begin(), filename_str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            filename_str.erase(std::find_if(filename_str.rbegin(), filename_str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), filename_str.end());

            if (!filename_str.empty()) {
                deleteFile(filename_str);
            }
            else {
                std::cerr << "Tên file không hợp lệ.\n";
            }
        }

    // Close the client socket
    closesocket(clientSocket);

    // Cleanup Winsock
    WSACleanup();

    system("pause");
    return 0;
}
