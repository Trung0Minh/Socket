﻿// AuthDialog.cpp
#include "AuthUI.h"
#include <wx/clipbrd.h>

BEGIN_EVENT_TABLE(AuthUI, wxDialog)
EVT_BUTTON(wxID_OK, AuthUI::OnOK)
EVT_BUTTON(wxID_CANCEL, AuthUI::OnCancel)
EVT_BUTTON(wxID_COPY, AuthUI::OnCopyUrl)
END_EVENT_TABLE()

AuthUI::AuthUI(wxWindow* parent, const wxString& authUrl)
    : wxDialog(parent, wxID_ANY, "Authorization Required",
        wxDefaultPosition, wxSize(400, 300))
{
    CreateControls(authUrl);
    Center();
}


void AuthUI::CreateControls(const wxString& authUrl)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Instructions
    wxStaticText* instructions = new wxStaticText(this, wxID_ANY,
        "Please follow these steps to authorize the application:");
    mainSizer->Add(instructions, 0, wxALL, 10);

    // Step 1
    wxStaticText* step1 = new wxStaticText(this, wxID_ANY,
        "1. Click the link below or copy the URL to visit the authorization page:");
    mainSizer->Add(step1, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    // URL and Copy button in horizontal layout
    wxBoxSizer* urlSizer = new wxBoxSizer(wxHORIZONTAL);
    // Sử dụng tên hiển thị ngắn gọn "Authorization Link" thay vì URL đầy đủ
    urlLink = new wxHyperlinkCtrl(this, wxID_ANY, "Authorization Link", authUrl);
    copyUrlButton = new wxButton(this, wxID_COPY, "Copy URL");

    urlSizer->Add(urlLink, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);  // Đã thay đổi proportion từ 1 thành 0
    urlSizer->Add(copyUrlButton, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    mainSizer->Add(urlSizer, 0, wxEXPAND | wxALL, 5);

    // Step 2
    wxStaticText* step2 = new wxStaticText(this, wxID_ANY,
        "2. Sign in to your account and authorize the application");
    mainSizer->Add(step2, 0, wxALL, 10);

    // Step 3
    wxStaticText* step3 = new wxStaticText(this, wxID_ANY,
        "3. Copy the authorization code and paste it below:");
    mainSizer->Add(step3, 0, wxALL, 10);

    // Auth code input
    authCodeInput = new wxTextCtrl(this, wxID_ANY, "",
        wxDefaultPosition, wxSize(-1, -1));
    mainSizer->Add(authCodeInput, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // Buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    okButton = new wxButton(this, wxID_OK, "Submit");
    cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");

    buttonSizer->Add(okButton, 0, wxALL, 5);
    buttonSizer->Add(cancelButton, 0, wxALL, 5);

    mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxALL, 10);

    SetSizer(mainSizer);
}

void AuthUI::OnOK(wxCommandEvent& event)
{
    authorizationCode = authCodeInput->GetValue().ToStdString();
    if (authorizationCode.empty()) {
        wxMessageBox("Please enter the authorization code.",
            "Error", wxOK | wxICON_ERROR);
        return;
    }

    if (validateCode) {
        std::string errorMessage;
        if (!validateCode(authorizationCode, errorMessage)) {
            wxMessageBox("Invalid authorization code: " + errorMessage + "\nPlease try again.",
                "Error", wxOK | wxICON_ERROR);
            authCodeInput->SetValue("");
            authCodeInput->SetFocus();
            return;
        }
    }

    EndModal(wxID_OK);
}

void AuthUI::OnCancel(wxCommandEvent& event)
{
    EndModal(wxID_CANCEL);
}

void AuthUI::OnCopyUrl(wxCommandEvent& event)
{
    if (wxTheClipboard->Open()) {
        auto data = new wxTextDataObject();
        data->SetText(urlLink->GetURL());
        wxTheClipboard->SetData(data);
        wxTheClipboard->Close();
        wxMessageBox("URL copied to clipboard!", "Success",
            wxOK | wxICON_INFORMATION);
    }
}