// ClientUI.cpp
#include "ClientUI.h"
#include <wx/regex.h>

// Constructor
ClientUI::ClientUI()
    : wxFrame(nullptr, wxID_ANY, "Email Client", wxDefaultPosition, wxSize(800, 600))
{
    client = new Client();
    // Set log callback ngay sau khi tạo client
    client->setLogCallback([this](const std::string& message) {
        AddLogMessage(wxString::FromUTF8(message));
        });

    isCompleted = false;

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

    if (client->connect(ip.ToStdString())) {
        connectedServers.push_back(std::make_pair(ip, true));
        UpdateServerList();
        completeButton->Enable(true);
        serverIpInput->SetValue("");
        serverIpInput->SetHint("Enter another server IP or click Complete");
    }
    else {
        // Không cần AddLogMessage ở đây vì Client đã tự log thông báo lỗi
        wxMessageBox("Connection failed! Please try again.", "Error", wxOK | wxICON_ERROR);
    }
}

void ClientUI::OnCompleteClick(wxCommandEvent& event) {
    if (!connectedServers.empty()) {
        isCompleted = true;
        EnableConnectionControls(false);
        // UI-specific log message có thể giữ lại
        AddLogMessage("Connection setup completed. Starting client...");
        wxString firstServerIP = connectedServers[0].first;
        client->start(firstServerIP.ToStdString());
    }
}

// Handles server disconnection
void ClientUI::HandleServerDisconnect(const wxString& ip) {
    auto it = std::find_if(connectedServers.begin(), connectedServers.end(),
        [&ip](const auto& pair) { return pair.first == ip; });

    if (it != connectedServers.end()) {
        connectedServers.erase(it);
        UpdateServerList();

        if (connectedServers.empty() && isCompleted) {
            isCompleted = false;
            EnableConnectionControls(true);
        }
    }

    disconnectButton->Enable(false);
}

// Handles disconnect button click event
void ClientUI::OnDisconnectClick(wxCommandEvent& event) {
    long selectedIndex = serverList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex != -1) {
        wxString ip = serverList->GetItemText(selectedIndex);
        client->disconnect();
        // Không cần AddLogMessage ở đây vì Client sẽ log việc disconnect

        auto it = std::find_if(connectedServers.begin(), connectedServers.end(),
            [&ip](const auto& pair) { return pair.first == ip; });
        if (it != connectedServers.end()) {
            connectedServers.erase(it);
        }

        UpdateServerList();
        disconnectButton->Enable(false);

        if (connectedServers.empty() && isCompleted) {
            isCompleted = false;
            EnableConnectionControls(true);
        }
    }
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

        int port = 54000 + static_cast<int>(i);
        if (port >= 54000 && port <= 65535) {
            serverList->SetItem(index, 1, wxString::Format(wxT("%d"), port));
        }
        else {
            serverList->SetItem(index, 1, wxT("Invalid Port"));
        }
    }

    serverList->Thaw();
    serverList->Refresh();
}

// Checks if a server is already connected
bool ClientUI::IsServerConnected(const wxString& ip) {
    return std::any_of(connectedServers.begin(), connectedServers.end(),
        [&ip](const auto& pair) { return pair.first == ip; });
}