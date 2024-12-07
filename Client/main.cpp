// main.cpp
#include "ClientUI.h"
#include "Auth.h"
#include <wx/wx.h>

class ClientApplication : public wxApp {
public:
    virtual bool OnInit() {
        // Khởi tạo Auth
        Auth auth("credentials.json", "token.json");

        try {
            // Kiểm tra xem đã authorize chưa
            if (!auth.authenticate()) {
                if (auth.isAuthenticationCancelled()) {
                    // Người dùng đã chủ động hủy xác thực
                    return false;  // Thoát ứng dụng một cách "êm đẹp"
                }
                else {
                    wxMessageBox("Authorization failed!", "Error", wxOK | wxICON_ERROR);
                    return false;
                }
            }

            // Xác thực thành công, tạo frame chính của ứng dụng
            ClientUI* frame = new ClientUI();

            // Hiển thị frame
            frame->Show(true);

            // Đặt frame này là window chính của ứng dụng
            SetTopWindow(frame);

            return true;

        }
        catch (const std::exception& e) {
            // Xử lý các lỗi khác có thể xảy ra trong quá trình xác thực
            wxMessageBox(
                wxString::Format("Authentication error: %s", e.what()),
                "Error",
                wxOK | wxICON_ERROR
            );
            return false;
        }
    }
};

// Khai báo ứng dụng wxWidgets
wxIMPLEMENT_APP(ClientApplication);