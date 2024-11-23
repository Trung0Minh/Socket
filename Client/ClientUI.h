// ClientUI.h
#pragma once
#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/listctrl.h>
#include <queue>
#include <mutex>
#include "Client.h"

class ClientUI : public wxFrame {
private:
    Client* client;
    bool isCompleted;
    std::queue<wxString> logMessages;
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

    // Layout
    wxBoxSizer* mainSizer;
    wxBoxSizer* connectionSizer;
    wxBoxSizer* serverListSizer;

    void CreateControls();
    void LayoutControls();
    void BindEvents();

    // Event handlers
    void OnConnectClick(wxCommandEvent& event);
    void OnCompleteClick(wxCommandEvent& event);
    void OnDisconnectClick(wxCommandEvent& event);
    void OnServerSelected(wxListEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnTimer(wxTimerEvent& event);

    bool ValidateIP(const wxString& ip);
    void UpdateLogArea();
    void SetupClientCallbacks();
    void UpdateServerList();
    bool IsServerConnected(const wxString& ip);
    void HandleServerDisconnect(const wxString& ip);

public:
    ClientUI();
    ~ClientUI();

    void AddLogMessage(const wxString& message);
    void EnableConnectionControls(bool enable);
};