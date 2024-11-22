// ClientUI.cpp
#include "ClientUI.h"
#include <wx/regex.h>

ClientUI::ClientUI()
    : wxFrame(nullptr, wxID_ANY, "Email Client",
        wxDefaultPosition, wxSize(800, 600))
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

void ClientUI::SetupClientCallbacks() {
    client->setLogCallback([this](const std::string& message) {
        AddLogMessage(wxString(message));
    });
}

ClientUI::~ClientUI() {
    if (client) {
        delete client;
    }
    if (updateTimer) {
        updateTimer->Stop();
        delete updateTimer;
    }
}

void ClientUI::CreateControls() {
    mainPanel = new wxPanel(this);

    // Connection controls
    serverIpInput = new wxTextCtrl(mainPanel, wxID_ANY,
        "Enter server IP", wxDefaultPosition, wxSize(200, -1));
    connectButton = new wxButton(mainPanel, wxID_ANY, "Connect");
    completeButton = new wxButton(mainPanel, wxID_ANY, "Complete Setup");
    completeButton->Enable(false);

    // Server list
    serverList = new wxListCtrl(mainPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 150),
        wxLC_REPORT | wxLC_SINGLE_SEL);
    serverList->InsertColumn(0, "Server IP", wxLIST_FORMAT_LEFT, 150);
    serverList->InsertColumn(1, "Status", wxLIST_FORMAT_LEFT, 100);

    disconnectButton = new wxButton(mainPanel, wxID_ANY, "Disconnect Selected");
    disconnectButton->Enable(false);

    // Log area
    logArea = new wxTextCtrl(mainPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);

    wxFont logFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    logArea->SetFont(logFont);
}

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

void ClientUI::BindEvents() {
    connectButton->Bind(wxEVT_BUTTON, &ClientUI::OnConnectClick, this);
    completeButton->Bind(wxEVT_BUTTON, &ClientUI::OnCompleteClick, this);
    disconnectButton->Bind(wxEVT_BUTTON, &ClientUI::OnDisconnectClick, this);
    serverList->Bind(wxEVT_LIST_ITEM_SELECTED, &ClientUI::OnServerSelected, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &ClientUI::OnClose, this);
    this->Bind(wxEVT_TIMER, &ClientUI::OnTimer, this);
}

bool ClientUI::ValidateIP(const wxString& ip) {
    wxRegEx ipRegex("^\\b((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b$");
    return ipRegex.Matches(ip);
}

void ClientUI::OnConnectClick(wxCommandEvent& event) {
    wxString ip = serverIpInput->GetValue();

    if (!ValidateIP(ip)) {
        wxMessageBox("Invalid IP address format!", "Error",
            wxOK | wxICON_ERROR);
        return;
    }

    if (IsServerConnected(ip)) {
        wxMessageBox("Already connected to this server!", "Warning",
            wxOK | wxICON_WARNING);
        return;
    }

    if (client->connect(ip.ToStdString())) {
        AddLogMessage("Successfully connected to " + ip);
        connectedServers.push_back(std::make_pair(ip, true));
        UpdateServerList();
        completeButton->Enable(true);
        serverIpInput->SetValue("Enter another server IP or click Complete");
    }
    else {
        AddLogMessage("Failed to connect to " + ip);
        wxMessageBox("Connection failed! Please try again.", "Error",
            wxOK | wxICON_ERROR);
    }
}

void ClientUI::OnCompleteClick(wxCommandEvent& event) {
    if (!connectedServers.empty()) {
        isCompleted = true;
        EnableConnectionControls(false);
        AddLogMessage("Connection setup completed. Starting client...");
        // Lấy IP của server đầu tiên để khởi động client
        wxString firstServerIP = connectedServers[0].first;
        client->start(firstServerIP.ToStdString());
    }
}

void ClientUI::OnDisconnectClick(wxCommandEvent& event) {
    long selectedIndex = serverList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedIndex != -1) {
        wxString ip = serverList->GetItemText(selectedIndex);
        client->disconnect();  // Gọi không cần tham số
        AddLogMessage("Disconnected from " + ip);

        // Remove from connected servers list
        auto it = std::find_if(connectedServers.begin(), connectedServers.end(),
            [&ip](const auto& pair) { return pair.first == ip; });
        if (it != connectedServers.end()) {
            connectedServers.erase(it);
        }

        UpdateServerList();
        disconnectButton->Enable(false);

        // If no servers left, enable connection controls
        if (connectedServers.empty() && isCompleted) {
            isCompleted = false;
            EnableConnectionControls(true);
        }
    }
}

void ClientUI::OnServerSelected(wxListEvent& event) {
    disconnectButton->Enable(true);
}

void ClientUI::OnClose(wxCloseEvent& event) {
    if (client) {
        client->stop();
    }
    event.Skip();
}

void ClientUI::OnTimer(wxTimerEvent& event) {
    UpdateLogArea();
}

void ClientUI::AddLogMessage(const wxString& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    logMessages.push(wxDateTime::Now().FormatTime() + ": " + message);
}

void ClientUI::UpdateLogArea() {
    std::lock_guard<std::mutex> lock(logMutex);
    while (!logMessages.empty()) {
        wxString message = logMessages.front();
        logMessages.pop();
        logArea->AppendText(message + "\n");
        logArea->ShowPosition(logArea->GetLastPosition());
    }
}

void ClientUI::EnableConnectionControls(bool enable) {
    serverIpInput->Enable(enable);
    connectButton->Enable(enable);
    completeButton->Enable(enable);
}

void ClientUI::UpdateServerList() {
    serverList->DeleteAllItems();
    for (size_t i = 0; i < connectedServers.size(); ++i) {
        const auto& server = connectedServers[i];
        long index = serverList->InsertItem(i, server.first);
        serverList->SetItem(index, 1, server.second ? "Connected" : "Disconnected");
    }
}

bool ClientUI::IsServerConnected(const wxString& ip) {
    return std::find_if(connectedServers.begin(), connectedServers.end(),
        [&ip](const auto& pair) { return pair.first == ip; }) != connectedServers.end();
}