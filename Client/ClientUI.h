// ClientUI.h
#pragma once
#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <vector>
#include <mutex>
#include <thread>
#include "Client.h"

class ClientUI : public wxFrame {
private:
    // Client and State Management
    Client* client;
    bool isCompleted;
    std::vector<wxString> logMessages;
    std::mutex logMutex;
    std::vector<std::pair<wxString, bool>> connectedServers;

    // UI Elements
    wxPanel* mainPanel;
    wxTextCtrl* serverIpInput;
    wxButton* connectButton;
    wxButton* completeButton;
    wxListCtrl* serverList;
    wxButton* disconnectButton;
    wxTextCtrl* logArea;
    wxTimer* updateTimer;

    // Layout Management
    wxBoxSizer* mainSizer;

    // UI Setup
    void CreateControls();
    void LayoutControls();
    void BindEvents();

    // Event Handlers
    void OnConnectClick(wxCommandEvent& event);
    void OnCompleteClick(wxCommandEvent& event);
    void OnDisconnectClick(wxCommandEvent& event);
    void OnServerSelected(wxListEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnTimer(wxTimerEvent& event);

    // Helper Functions
    bool ValidateIP(const wxString& ip);
    void UpdateLogArea();
    void UpdateServerList();
    bool IsServerConnected(const wxString& ip);
    void HandleServerDisconnect(const wxString& ip);

    wxStaticBox* serverListBox;
    wxStaticBox* connectionBox;

public:
    // Constructor and Destructor
    ClientUI();
    ~ClientUI();

    // Log Management
    void AddLogMessage(const wxString& message);

    // UI Control Management
    void EnableConnectionControls(bool enable);
};