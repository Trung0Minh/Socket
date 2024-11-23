// main.cpp
#include "ClientUI.h"
#include <wx/wx.h>

// Định nghĩa lớp Application
class ClientApplication : public wxApp {
public:
    virtual bool OnInit() {
        // Tạo frame chính của ứng dụng
        ClientUI* frame = new ClientUI();

        // Hiển thị frame
        frame->Show(true);

        // Đặt frame này là window chính của ứng dụng
        SetTopWindow(frame);

        return true;
    }
};

// Khai báo ứng dụng wxWidgets
wxIMPLEMENT_APP(ClientApplication);