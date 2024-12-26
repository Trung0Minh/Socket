# Lấy file `credentials.json` trên Google Cloud Console

Để sử dụng Gmail API hoặc bất kỳ dịch vụ API nào của Google, bạn cần file `credentials.json`. File này chứa các thông tin xác thực cần thiết để ứng dụng của bạn kết nối với API. Dưới đây là hướng dẫn chi tiết từng bước để lấy file `credentials.json`:

---

## Bước 1: Truy cập Google Cloud Console
1. Mở trình duyệt và truy cập vào [Google Cloud Console](https://console.cloud.google.com/).
2. Đăng nhập bằng tài khoản Google của bạn.

---

## Bước 2: Tạo hoặc chọn dự án
1. Ở góc trên bên trái, nhấn vào nút **Select a Project**.
2. Nhấn **New Project** nếu bạn chưa có dự án, hoặc chọn một dự án hiện có.

---

## Bước 3: Bật Gmail API
1. Trong giao diện của dự án, tìm menu bên trái, chọn **APIs & Services** > **Library**.
2. Tìm kiếm **Gmail API** bằng cách gõ vào ô tìm kiếm.
3. Chọn **Gmail API** và nhấn nút **Enable** để bật API.

---

## Bước 4: Tạo thông tin đăng nhập (Credentials)
1. Chọn **APIs & Services** > **Credentials** từ menu bên trái.
2. Nhấn nút **Create Credentials** và chọn **OAuth 2.0 Client IDs**.
3. Nếu chưa cấu hình màn hình cấp quyền (OAuth consent screen):
   - Nhấn vào **Configure consent screen**.
   - Chọn loại ứng dụng là **External** và nhấn **Create**.
   - Điền các thông tin bắt buộc:
     - **App name**: Tên ứng dụng của bạn.
     - **User support email**: Email hỗ trợ (email của bạn).
     - **Developer contact information**: Email nhà phát triển (email của bạn).
   - Nhấn **Save and Continue** để hoàn tất.
4. Sau khi cấu hình màn hình OAuth, quay lại bước tạo thông tin đăng nhập:
   - Chọn **Application type** là **Desktop app**.
   - Điền tên bất kỳ cho client (ví dụ: `MyDesktopApp`).
   - Nhấn **Create**.

---

## Bước 5: Tải file `credentials.json`
1. Sau khi tạo thành công, bạn sẽ thấy một cửa sổ hiện ra với các thông tin Client ID và Client Secret.
2. Nhấn nút **Download JSON** để tải file `credentials.json`.
3. Lưu file này vào thư mục dự án của bạn, thường là cùng cấp với mã nguồn chính (ví dụ: `src/credentials.json`).

---

## Lưu ý
- Không chia sẻ file `credentials.json` với người khác. File này chứa thông tin nhạy cảm có thể bị lạm dụng.
- Nếu bạn vô tình để lộ file này trên GitHub hoặc nơi công khai, hãy xoá và tạo mới trong Google Cloud Console.
- Đảm bảo rằng bạn đã cài đặt và định cấu hình quyền hạn cho ứng dụng sử dụng API (OAuth scopes).

---

## Liên hệ hỗ trợ
Nếu gặp bất kỳ vấn đề nào, bạn có thể tham khảo:
- [Google Cloud Console Documentation](https://cloud.google.com/docs)
- [Gmail API Quickstart](https://developers.google.com/gmail/api/quickstart)
