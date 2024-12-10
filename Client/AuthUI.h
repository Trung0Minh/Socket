// AuthDialog.h
#pragma once
#ifndef AUTH_DIALOG_H
#define AUTH_DIALOG_H

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/hyperlink.h>  // Thêm include cho wxHyperlinkCtrl
#include <string>

class AuthDialog : public wxDialog {
public:
    AuthDialog(wxWindow* parent, const wxString& authUrl);
    std::string GetAuthorizationCode() const { return authorizationCode; }

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

    DECLARE_EVENT_TABLE()
};

#endif // AUTH_DIALOG_H