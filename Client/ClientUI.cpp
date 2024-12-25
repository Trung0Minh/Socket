// ClientUI.cpp
#include "ClientUI.h"
#include <wx/regex.h>

// Constructor
ClientUI::ClientUI() : wxFrame(nullptr, wxID_ANY, "Client", wxDefaultPosition, wxSize(800, 600)) {
    client = new Client();
    client->setLogCallback([this](const std::string& msg) { AddLogMessage(wxString::FromUTF8(msg)); });

    CreateControls();
    LayoutControls();
    BindEvents();

    updateTimer = new wxTimer(this);
    updateTimer->Start(100);
    Center();
}

// Destructor
ClientUI::~ClientUI() {
    delete client;
    delete updateTimer;
}

// Creates and initializes UI controls
void ClientUI::CreateControls() {
    mainPanel = new wxPanel(this);

    // Server List Box
    serverListBox = new wxStaticBox(mainPanel, wxID_ANY, "Connected Servers");
    serverList = new wxListCtrl(serverListBox, wxID_ANY, wxDefaultPosition, wxSize(300, 150),
        wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    serverList->InsertColumn(0, "Server IP", wxLIST_FORMAT_LEFT, 150);
    serverList->InsertColumn(1, "Port", wxLIST_FORMAT_LEFT, 100);
    disconnectButton = new wxButton(serverListBox, wxID_ANY, "Disconnect server");
    disconnectButton->Enable(false);

    // Connection Box
    connectionBox = new wxStaticBox(mainPanel, wxID_ANY, "Enter IP address");
    serverIpInput = new wxTextCtrl(connectionBox, wxID_ANY, "",
        wxDefaultPosition, wxSize(200, -1), wxTE_PROCESS_ENTER);
    serverIpInput->SetHint("Enter server IP");
    connectButton = new wxButton(connectionBox, wxID_ANY, "Connect");
    completeButton = new wxButton(connectionBox, wxID_ANY, "Complete");
    completeButton->Enable(false);
    helpButton = new wxButton(connectionBox, wxID_ANY, "Help");

    aboutButton = new wxButton(connectionBox, wxID_ANY, "About");

    // Log Area
    logArea = new wxTextCtrl(mainPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
    wxFont logFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    logArea->SetFont(logFont);
}

void ClientUI::LayoutControls() {
    mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

    // Server List Layout
    wxStaticBoxSizer* serverListSizer = new wxStaticBoxSizer(serverListBox, wxVERTICAL);
    serverListSizer->Add(serverList, 1, wxEXPAND | wxALL, 5);
    serverListSizer->Add(disconnectButton, 0, wxALL | wxALIGN_CENTER, 5);

    // Connection Controls Layout
    wxStaticBoxSizer* connectionSizer = new wxStaticBoxSizer(connectionBox, wxVERTICAL);
    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
    inputSizer->Add(serverIpInput, 1, wxALL | wxEXPAND, 5);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(connectButton, 0, wxALL, 5);
    buttonSizer->Add(completeButton, 0, wxALL, 5);
    buttonSizer->Add(aboutButton, 0, wxALL, 5);
    buttonSizer->Add(helpButton, 0, wxALL, 5);

    connectionSizer->Add(inputSizer, 0, wxEXPAND);
    connectionSizer->Add(buttonSizer, 0, wxALIGN_RIGHT);

    // Arrange top section
    topSizer->Add(serverListSizer, 1, wxEXPAND | wxALL, 5);
    topSizer->Add(connectionSizer, 1, wxEXPAND | wxALL, 5);

    // Main layout
    mainSizer->Add(topSizer, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(logArea, 1, wxEXPAND | wxALL, 5);

    mainPanel->SetSizer(mainSizer);
}

// Binds event handlers to UI elements
void ClientUI::BindEvents() {
    serverIpInput->Bind(wxEVT_TEXT_ENTER, &ClientUI::OnConnectClick, this);
    connectButton->Bind(wxEVT_BUTTON, &ClientUI::OnConnectClick, this);
    completeButton->Bind(wxEVT_BUTTON, &ClientUI::OnCompleteClick, this);
    aboutButton->Bind(wxEVT_BUTTON, &ClientUI::OnAboutClick, this);
    helpButton->Bind(wxEVT_BUTTON, &ClientUI::OnHelpClick, this);
    disconnectButton->Bind(wxEVT_BUTTON, &ClientUI::OnDisconnectClick, this);
    serverList->Bind(wxEVT_LIST_ITEM_SELECTED, &ClientUI::OnServerSelected, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &ClientUI::OnClose, this);
    this->Bind(wxEVT_TIMER, &ClientUI::OnTimer, this);
}

// Validates IP address format
bool ClientUI::ValidateIP(const wxString& ip) {
    wxRegEx ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.Matches(ip);
}

void ClientUI::OnAboutClick(wxCommandEvent& event) {
    wxDialog* aboutDialog = new wxDialog(this, wxID_ANY, "About",
        wxDefaultPosition, wxSize(400, 250));

    wxBoxSizer* dialogSizer = new wxBoxSizer(wxVERTICAL);

    // Tạo panel cho dialog
    wxPanel* panel = new wxPanel(aboutDialog);
    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);

    // Thêm nội dung
    wxStaticText* title = new wxStaticText(panel, wxID_ANY,
        "Computer Networking - 23TNT1");
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(titleFont);

    wxStaticText* teacher = new wxStaticText(panel, wxID_ANY,
        "Instructor: Do Hoang Cuong");
    wxStaticText* memberTitle = new wxStaticText(panel, wxID_ANY,
        "Members:");
    wxStaticText* member1 = new wxStaticText(panel, wxID_ANY,
        "23122014: Hoang Minh Trung");
    wxStaticText* member2 = new wxStaticText(panel, wxID_ANY,
        "23122021: Bui Duy Bao");
    wxStaticText* member3 = new wxStaticText(panel, wxID_ANY,
        "23122025: Pham Ngoc Duy");

    // Tạo BoxSizer ngang để chứa text "Demo" và hyperlink
    wxBoxSizer* demoSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* demoText = new wxStaticText(panel, wxID_ANY, "Demo: ");
    wxHyperlinkCtrl* demoLink = new wxHyperlinkCtrl(panel, wxID_ANY,
        "https://www.youtube.com/watch?v=dQw4w9WgXcQ&pp=ygUJcmljayByb2xs",
        "https://www.youtube.com/watch?v=dQw4w9WgXcQ&pp=ygUJcmljayByb2xs");

    demoSizer->Add(demoText, 0, wxALIGN_CENTER_VERTICAL);
    demoSizer->Add(demoLink, 0, wxALIGN_CENTER_VERTICAL);

    // Thêm các thành phần vào panelSizer
    panelSizer->Add(title, 0, wxALL, 5);
    panelSizer->Add(teacher, 0, wxALL, 5);
    panelSizer->Add(memberTitle, 0, wxALL, 5);
    panelSizer->Add(member1, 0, wxLEFT, 15);
    panelSizer->Add(member2, 0, wxLEFT, 15);
    panelSizer->Add(member3, 0, wxLEFT, 15);
    panelSizer->AddSpacer(10);
    panelSizer->Add(demoSizer, 0, wxALL, 5);

    panel->SetSizer(panelSizer);

    // Thêm nút OK
    wxButton* okButton = new wxButton(aboutDialog, wxID_OK, "OK");
    dialogSizer->Add(panel, 1, wxEXPAND | wxALL, 5);
    dialogSizer->Add(okButton, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    aboutDialog->SetSizer(dialogSizer);
    aboutDialog->Center();
    aboutDialog->ShowModal();
    aboutDialog->Destroy();
}

void ClientUI::OnHelpClick(wxCommandEvent& event) {
    wxDialog* helpDialog = new wxDialog(this, wxID_ANY, "Help Guide",
        wxDefaultPosition, wxSize(600, 700));

    wxBoxSizer* dialogSizer = new wxBoxSizer(wxVERTICAL);

    wxScrolledWindow* scrolledWindow = new wxScrolledWindow(helpDialog, wxID_ANY);
    scrolledWindow->SetScrollRate(5, 5);

    wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);

    // Tạo panel chứa tiêu đề với nền màu nhạt
    wxPanel* titlePanel = new wxPanel(scrolledWindow, wxID_ANY);
    titlePanel->SetBackgroundColour(wxColour(240, 240, 240));  // Màu nền xám nhạt

    // Tạo tiêu đề Welcome với titlePanel là parent
    wxStaticText* welcomeTitle = new wxStaticText(titlePanel, wxID_ANY,
        "Welcome to the Client Application!");

    // Tùy chỉnh font cho tiêu đề
    wxFont titleFont = welcomeTitle->GetFont();
    titleFont.SetPointSize(16);  // Kích thước font lớn hơn
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);  // Chữ đậm
    welcomeTitle->SetFont(titleFont);
    welcomeTitle->SetForegroundColour(wxColour(0, 102, 204));  // Màu xanh dương

    wxBoxSizer* titlePanelSizer = new wxBoxSizer(wxVERTICAL);
    titlePanelSizer->Add(welcomeTitle, 0, wxALIGN_CENTER | wxALL, 15);
    titlePanel->SetSizer(titlePanelSizer);

    // Thêm nội dung chính
    wxTextCtrl* helpText = new wxTextCtrl(scrolledWindow, wxID_ANY,
        "This application allows you to connect to servers, send and receive data, and view logs. "
        "Below is a brief guide on how to use the application:\n\n"
        "1. *Enter Server IP*: Enter the IP address of the server you wish to connect to in the 'Enter IP address' field.\n"
        "2. *Connect to Server*: Click the 'Connect' button to initiate the connection to the server.\n"
        "3. *Disconnect from Server*: Once connected, you can disconnect from the server by selecting the server from the list and clicking 'Disconnect server'.\n"
        "4. *Complete Setup*: After connecting, you can complete the setup by clicking 'Complete'. This will finalize the connection process and start the client.\n"
        "5. *Log Area*: All activities such as connection status, disconnections, and any errors will be displayed in the 'Log Area'.\n"
        "6. *About*: Click the 'About' button to learn more about the application.\n\n"
        "For any further assistance, feel free to reach out to our support team.\n\n"
        "Here is the list of commands you can relate\n\n"
        "* Applications:\n"
        "- list applications : List the currently running applications\n"
        "- startApps + application name: Open an application(can include or exclude the .exe extension)\n"
        "- stopApps + PID: Close an application using its PID that is available in the list of applications\n\n"
        "* Services:\n"
        "- list services : List the services that are currently running or stopped\n"
        "- startServ + service name: Start a service(no need to include the extension)\n"
        "- stopServ + service name: Stop a service by its name(same as startServ)\n\n"
        "* Process:\n"
        "- list processes : List the processes that are currently running\n"
        "- stopProc + PID: Terminate a process using its PID that is available in the list of processes\n\n"
        "- list ip : List the network IP addresses of the server\n\n"
        "- shutdown: Shut down the server machine\n\n"
        "- restart: Restart the server machine\n\n"
        "- screenshot: Capture a screenshot of the server's screen\n\n"
        "- record + duration in seconds : Start the camera and record for the specified duration\n\n"
        "- sendfile + absolute file path on the server: Retrieve a file from the server\n\n"
        "- delete + absolute file path on the server: Delete a file on the server\n",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    // Thiết lập font cho nội dung chính
    wxFont helpFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    helpText->SetFont(helpFont);

    // Thêm các thành phần vào content sizer
    contentSizer->Add(titlePanel, 0, wxEXPAND | wxALL, 0);  // Thêm panel tiêu đề
    contentSizer->Add(helpText, 1, wxEXPAND | wxALL, 10);   // Thêm nội dung chính

    scrolledWindow->SetSizer(contentSizer);

    // Tính toán kích thước tối thiểu cần thiết
    wxSize minSize = contentSizer->GetMinSize();
    scrolledWindow->SetVirtualSize(minSize);

    // Thêm nút OK
    wxButton* okButton = new wxButton(helpDialog, wxID_OK, "OK");

    dialogSizer->Add(scrolledWindow, 1, wxEXPAND | wxALL, 5);
    dialogSizer->Add(okButton, 0, wxALIGN_CENTER | wxALL, 10);

    helpDialog->SetSizer(dialogSizer);

    // Đặt kích thước tối thiểu cho dialog
    helpDialog->SetMinSize(wxSize(500, 600));

    helpDialog->Center();
    helpDialog->ShowModal();
    helpDialog->Destroy();
}

// Handles connect button click event
void ClientUI::OnConnectClick(wxCommandEvent& event) {
    wxString ip = serverIpInput->GetValue();

    if (!ValidateIP(ip)) {
        wxMessageBox("Invalid IP address format!", "Error", wxOK | wxICON_ERROR);
        return;
    }

    if (IsServerConnected(ip)) {
        wxMessageBox("Already connected to this server!", "Warning", wxOK | wxICON_WARNING);
        return;
    }

    // Disable controls during connection attempt
    EnableConnectionControls(false);

    // Create connection thread
    std::thread([this, ip]() {
        bool connected = client->connect(ip.ToStdString());

        // Use CallAfter to update UI from the main thread
        wxTheApp->CallAfter([this, connected, ip]() {
            EnableConnectionControls(true);

            if (connected) {
                connectedServers.push_back(std::make_pair(ip, true));
                UpdateServerList();
                completeButton->Enable(true);
                serverIpInput->SetValue("");
                serverIpInput->SetHint("Enter another server IP or click Complete");
            }
            else {
                wxMessageBox("Connection failed! Server might be offline",
                    "Error", wxOK | wxICON_ERROR);
            }
            });
        }).detach();
}

void ClientUI::OnCompleteClick(wxCommandEvent& event) {
    if (!connectedServers.empty()) {
        isCompleted = true;
        EnableConnectionControls(false);
        AddLogMessage("Connection setup completed. Starting client...");
        client->start(connectedServers[0].first.ToStdString());
    }
}

// Handles disconnect button click event
void ClientUI::OnDisconnectClick(wxCommandEvent& event) {
    long selectedIndex = serverList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex != -1) {
        HandleServerDisconnect(serverList->GetItemText(selectedIndex));
    }
}

// Handles server disconnection
void ClientUI::HandleServerDisconnect(const wxString& ip) {
    client->disconnect();
    auto it = std::find_if(connectedServers.begin(), connectedServers.end(),
        [&ip](const auto& server) { return server.first == ip; });

    if (it != connectedServers.end()) {
        connectedServers.erase(it);
        UpdateServerList();
        AddLogMessage("Server disconnected " + ip);

        if (connectedServers.empty() && isCompleted) {
            isCompleted = false;
            EnableConnectionControls(true);
        }
    }
    disconnectButton->Enable(false);
}

// Handles server selection event
void ClientUI::OnServerSelected(wxListEvent& event) {
    disconnectButton->Enable(true);
}

// Handles window close event
void ClientUI::OnClose(wxCloseEvent& event) {
    if (client) {
        client->stop();
    }
    event.Skip();
}

// Timer event handler to update log area
void ClientUI::OnTimer(wxTimerEvent& event) {
    UpdateLogArea();
    if (client && !connectedServers.empty() && !client->isConnected()) {
        HandleServerDisconnect(connectedServers[0].first);
    }
}

// Adds a log message to the log queue
void ClientUI::AddLogMessage(const wxString& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    logMessages.push_back(wxDateTime::Now().FormatTime() + ": " + message);
}

// Updates the log area with messages from the queue
void ClientUI::UpdateLogArea() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!logMessages.empty()) {
        for (const wxString& message : logMessages) {
            logArea->AppendText(message + "\n");
        }
        logArea->ShowPosition(logArea->GetLastPosition());
        logMessages.clear();
    }
}

// Enables or disables connection controls
void ClientUI::EnableConnectionControls(bool enable) {
    serverIpInput->Enable(enable);
    connectButton->Enable(enable);
    completeButton->Enable(enable);
}

// Updates the server list in the UI
void ClientUI::UpdateServerList() {
    serverList->Freeze();
    serverList->DeleteAllItems();

    for (size_t i = 0; i < connectedServers.size(); ++i) {
        const auto& server = connectedServers[i];
        long index = serverList->InsertItem(i, server.first);
        serverList->SetItem(index, 1, wxString::Format("%d", 54000 + static_cast<int>(i)));
    }

    serverList->Thaw();
    serverList->Refresh();
}

// Checks if a server is already connected
bool ClientUI::IsServerConnected(const wxString& ip) {
    return std::any_of(connectedServers.begin(), connectedServers.end(),
        [&ip](const auto& pair) { return pair.first == ip; });
}