#define _CRT_NO_WARNINGS
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

#include <opencv2/opencv.hpp>

std::wstring convertToWString(const CHAR* str) {
    size_t len = strlen(str) + 1;
    wchar_t* wstr = new wchar_t[len];
    size_t outSize;
    mbstowcs_s(&outSize, wstr, len, str, len - 1);
    std::wstring wresult(wstr);
    delete[] wstr;
    return wresult;
}

void listRunningApplications() {
    std::set<std::pair<std::wstring, DWORD>> appDetails;

    // Create snapshot of all running processes
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
        // Open process to retrieve its main window handle
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            HWND hWnd = GetTopWindow(NULL);
            while (hWnd) {
                DWORD processID;
                GetWindowThreadProcessId(hWnd, &processID);
                if (processID == pe32.th32ProcessID && GetWindow(hWnd, GW_OWNER) == NULL && IsWindowVisible(hWnd)) {
                    appDetails.insert({ convertToWString(pe32.szExeFile), pe32.th32ProcessID });
                    break;
                }
                hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    // Print unique application names with their IDs
    int cnt = 1;
    for (const auto& app : appDetails) {
        std::wcout << cnt++ << L". " << app.first << L" (ID: " << app.second << L")" << std::endl;
    }
}

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


bool closeApplicationById(DWORD processId) {
    // Mở quá trình với quyền terminate
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Unable to open process with ID " << processId << ". Error: " << GetLastError() << std::endl;
        return false;
    }

    // Đóng quá trình
    if (TerminateProcess(hProcess, 0) == 0) {
        std::cerr << "Failed to terminate process with ID " << processId << ". Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);
    std::cout << "Successfully closed the application with ID " << processId << "." << std::endl;
    return true;
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

const std::string SAVE_PATH = "D:\\MMT\\Do an\\vid\\output_video.mp4"; // Đường dẫn cố định cho file
void recordVideoFromCamera() {
    cv::VideoCapture camera(0); // Mở camera mặc định (camera ID là 0)
    if (!camera.isOpened()) {
        std::cerr << "Could not open the camera." << std::endl;
        return;
    }

    int frameWidth = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(camera.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::Size frameSize(frameWidth, frameHeight);

    // Tạo đối tượng VideoWriter để lưu video tại SAVE_PATH
    cv::VideoWriter videoWriter(SAVE_PATH, cv::VideoWriter::fourcc('H', '2', '6', '4'), 30, frameSize);

    if (!videoWriter.isOpened()) {
        std::cerr << "Could not open the video writer." << std::endl;
        return;
    }

    std::cout << "Recording video..." << std::endl;

    int duration = 3; // Thời gian quay video (giây)
    int frameCount = duration * 30; // 30 FPS (số khung hình mỗi giây)

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
    std::cout << "Video recording completed and saved at " << SAVE_PATH << std::endl;
}

#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

struct AppInfo {
    int index;
    std::wstring name;
    int processID;
    int threadCount;
};
std::vector<AppInfo> getApps() {
    std::vector<AppInfo> appList;

    // Initialize COM library
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "Failed to initialize COM library." << std::endl;
        return appList;
    }

    // Set general COM security levels
    hres = CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
    );

    if (FAILED(hres)) {
        std::cerr << "Failed to initialize security." << std::endl;
        CoUninitialize();
        return appList;
    }

    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc);

    if (FAILED(hres)) {
        std::cerr << "Failed to create IWbemLocator object." << std::endl;
        CoUninitialize();
        return appList;
    }

    // Connect to WMI
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // WMI namespace
        NULL,                    // User name
        NULL,                    // User password
        0,                       // Locale
        NULL,                    // Security flags
        0,                       // Authority
        0,                       // Context object
        &pSvc                    // IWbemServices proxy
    );

    if (FAILED(hres)) {
        std::cerr << "Could not connect to WMI namespace ROOT\\CIMV2." << std::endl;
        pLoc->Release();
        CoUninitialize();
        return appList;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hres)) {
        std::cerr << "Could not set proxy blanket." << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return appList;
    }

    // Query for running processes
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Name, ProcessId, ThreadCount FROM Win32_Process WHERE Name IS NOT NULL"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        std::cerr << "Query for processes failed." << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return appList;
    }

    // Retrieve data from query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    int index = 1;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtName, vtProcessId, vtThreadCount;
        pclsObj->Get(L"Name", 0, &vtName, 0, 0);
        pclsObj->Get(L"ProcessId", 0, &vtProcessId, 0, 0);
        pclsObj->Get(L"ThreadCount", 0, &vtThreadCount, 0, 0);

        AppInfo app;
        app.index = index++;
        app.name = vtName.bstrVal;
        app.processID = vtProcessId.intVal;
        app.threadCount = vtThreadCount.intVal;
        appList.push_back(app);

        VariantClear(&vtName);
        VariantClear(&vtProcessId);
        VariantClear(&vtThreadCount);
        pclsObj->Release();
    }

    // Cleanup
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    return appList;
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
        ZeroMemory(buf, 4096);
        // Wait for client to send data
        int bytesReceived = recv(clientSocket, buf, 4096, 0);
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
            listRunningApplications();
        }
        else if (std::string(buf, 0, bytesReceived) == "remove") {
            // Yêu cầu người dùng nhập ID của ứng dụng muốn đóng
            DWORD pidToClose;
            std::wcout << L"Enter the Process ID (PID) to close: ";
            std::wcin >> pidToClose;

            // Gọi hàm để đóng ứng dụng
            closeApplicationById(pidToClose);
        }
        else if (std::string(buf, 0, bytesReceived) == "shutdown") {
            shutdownComputer();
        }
        else if (std::string(buf, 0, bytesReceived) == "start") {
            // Liệt kê các ứng dụng đang chạy
            listRunningApplications();

            // Yêu cầu người dùng nhập ID của ứng dụng muốn mở
            DWORD pidToOpen;
            std::wcout << L"Enter the Process ID (PID) to open: ";
            std::wcin >> pidToOpen;

            // Gọi hàm để mở ứng dụng
            openApplicationById(pidToOpen);
        }
        else if (std::string(buf, 0, bytesReceived) == "camera") {
            recordVideoFromCamera();
        }
        else if (std::string(buf, 0, bytesReceived) == "get") {
            std::vector<AppInfo> apps = getApps();

            std::wcout << L"No.  Description                  Id     ThreadCount" << std::endl;
            std::wcout << L"---------------------------------------------" << std::endl;

            for (const auto& app : apps) {
                std::wcout << app.index << L"    "
                    << app.name << L"    "
                    << app.processID << L"    "
                    << app.threadCount << std::endl;
            }
        }
        // Echo message back to client
        send(clientSocket, buf, bytesReceived + 1, 0);
    }

    // Close the client socket
    closesocket(clientSocket);

    // Cleanup Winsock
    WSACleanup();

    system("pause");
    return 0;
}
