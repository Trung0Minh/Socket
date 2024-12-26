# Lấy file `credentials.json` trên Google Cloud Console

Để sử dụng Gmail API hoặc bất kỳ dịch vụ API nào của Google, bạn cần file `credentials.json`. File này chứa các thông tin xác thực cần thiết để ứng dụng của bạn kết nối với API. Dưới đây là hướng dẫn để lấy file `credentials.json`:

1. Mở trình duyệt và truy cập vào [Google Cloud Console](https://console.cloud.google.com/).
2. Đăng nhập bằng tài khoản Google của bạn.
3. Tạo Project mới.
4. Truy cập **APIs & Services** > **Library**, tìm kiếm **Gmail API** và nhấn nút **Enable**.
5. Chọn **APIs & Services** > **Credentials** từ menu bên trái.
6. Nhấn nút **Create Credentials** và chọn **OAuth 2.0 Client IDs**.
7. Thực hiện cấu hình OAuth consent screen, link Youtube hướng dẫn: https://www.youtube.com/watch?v=1Ua0Eplg75M
8. Sau khi tạo thành công, bạn sẽ thấy một cửa sổ hiện ra với các thông tin Client ID và Client Secret.
9. Nhấn nút **Download JSON** để tải file `credentials.json`.
10. Lưu file này vào thư mục dự án của bạn, thường là cùng cấp với mã nguồn chính.

Hãy nhớ thêm địa chỉ gmail vào danh sách **Test users** nếu dự án vẫn còn ở chế độ **Testing**.

---
# Config

Sau khi thực hiện lấy file `credentials.json`, mở `Config.cpp` và dán **Client ID** và **Client Secret**, cùng với path để lưu file `token.json` (thường mặc định là "token.json" - cùng cấp với mã nguồn chính).

---
# Yêu cầu về thư viện

## Client
1. WxWidgets: https://www.wxwidgets.org
2. 
