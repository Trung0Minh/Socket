// AuthUI.h
#pragma once
#ifndef AUTH_DIALOG_H
#define AUTH_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/hyperlink.h>
#include <string>

class AuthUI : public wxDialog {
public:
    AuthUI(wxWindow* parent, const wxString& authUrl);
    std::string GetAuthorizationCode() const { return authorizationCode; }
    void SetValidationCallback(std::function<bool(const std::string&, std::string&)> callback) {
        validateCode = callback;
    }

private:
    void CreateControls(const wxString& authUrl);
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnCopyUrl(wxCommandEvent& event);

    wxTextCtrl* authCodeInput;
    wxButton* okButton;
    wxButton* cancelButton;
    wxButton* copyUrlButton;
    wxHyperlinkCtrl* urlLink;
    std::string authorizationCode;
    std::function<bool(const std::string&, std::string&)> validateCode;

    DECLARE_EVENT_TABLE()
};

#endif // AUTH_DIALOG_H