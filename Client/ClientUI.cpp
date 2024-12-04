// ClientUI.cpp
#include "ClientUI.h"
#include <wx/regex.h>

// Constructor
ClientUI::ClientUI()
    : wxFrame(nullptr, wxID_ANY, "Email Client", wxDefaultPosition, wxSize(800, 600))
{
    client = new Client();
    isCompleted = false;

    CreateControls();
    LayoutControls();
    BindEvents();
    SetupClientCallbacks();

    updateTimer = new wxTimer(this);
    updateTimer->Start(100);

    Center();
}

// Sets up client callbacks for logging and reconnecting
void ClientUI::SetupClientCallbacks() {
    client->setLogCallback([this](const std::string& message) {
        wxString wxMessage(message);
        AddLogMessage(wxMessage);

        // Handle disconnection messages
        if (wxMessage.Contains("Disconnected") || wxMessage.Contains("Connection lost")) {
            HandleServerDisconnect(wxMessage.AfterLast(' ').BeforeLast('.'));
        }
        });

    // Handle server reconnection
    client->setReconnectCallback([this](const std::string& ip) {
        CallAfter([this, ip]() {
            wxString wxIP(ip);
            if (!IsServerConnected(wxIP)) {
                connectedServers.push_back(std::make_pair(wxIP, true));
                UpdateServerList();
            }
            });
        });
}

// Destructor
ClientUI::~ClientUI() {
    delete client;
    delete updateTimer;
}

// Creates and initializes UI controls
void ClientUI::CreateControls() {
    mainPanel = new wxPanel(this);

    // Thêm cờ wxTE_PROCESS_ENTER
    serverIpInput = new wxTextCtrl(mainPanel, wxID_ANY, "Enter server IP", wxDefaultPosition, wxSize(200, -1), wxTE_PROCESS_ENTER);
    connectButton = new wxButton(mainPanel, wxID_ANY, "Connect");
    completeButton = new wxButton(mainPanel, wxID_ANY, "Complete");
    completeButton->Enable(false);

    serverList = new wxListCtrl(mainPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 150), wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    serverList->InsertColumn(0, "Server IP", wxLIST_FORMAT_LEFT, 150);
    serverList->InsertColumn(1, "Status", wxLIST_FORMAT_LEFT, 100);

    disconnectButton = new wxButton(mainPanel, wxID_ANY, "Disconnect Selected");
    disconnectButton->Enable(false);

    logArea = new wxTextCtrl(mainPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
    wxFont logFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    logArea->SetFont(logFont);
}

// Arranges UI controls using sizers
void ClientUI::LayoutControls() {
    mainSizer = new wxBoxSizer(wxVERTICAL);
    connectionSizer = new wxBoxSizer(wxHORIZONTAL);
    serverListSizer = new wxBoxSizer(wxHORIZONTAL);

    connectionSizer->Add(serverIpInput, 1, wxALL | wxEXPAND, 5);
    connectionSizer->Add(connectButton, 0, wxALL, 5);
    connectionSizer->Add(completeButton, 0, wxALL, 5);

    serverListSizer->Add(serverList, 1, wxEXPAND | wxALL, 5);
    serverListSizer->Add(disconnectButton, 0, wxALL, 5);

    mainSizer->Add(connectionSizer, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(serverListSizer, 0, wxEXPAND | wxALL, 5);
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
        AddLogMessage("Failed to connect to " + ip);
        wxMessageBox("Connection failed! Please try again.", "Error", wxOK | wxICON_ERROR);
    }
}

// Handles complete button click event
void ClientUI::OnCompleteClick(wxCommandEvent& event) {
    if (!connectedServers.empty()) {
        isCompleted = true;
        EnableConnectionControls(false);
        AddLogMessage("Connection setup completed. Starting client...");
        wxString firstServerIP = connectedServers[0].first;
        client->start(firstServerIP.ToStdString());
    }
}

// Handles server disconnection
void ClientUI::HandleServerDisconnect(const wxString& ip) {
    auto it = std::find_if(connectedServers.begin(), connectedServers.end(), [&ip](const auto& pair) { return pair.first == ip; });

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
        AddLogMessage("Disconnected from " + ip);

        auto it = std::find_if(connectedServers.begin(), connectedServers.end(), [&ip](const auto& pair) { return pair.first == ip; });
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
    for (const wxString& message : logMessages) {
        logArea->AppendText(message + "\n");
        logArea->ShowPosition(logArea->GetLastPosition());
    }
    logMessages.clear();
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
        serverList->SetItem(index, 1, server.second ? "Connected" : "Disconnected");
    }

    serverList->Thaw();
    serverList->Refresh();
}

// Checks if a server is already connected
bool ClientUI::IsServerConnected(const wxString& ip) {
    return std::any_of(connectedServers.begin(), connectedServers.end(), [&ip](const auto& pair) { return pair.first == ip; });
}