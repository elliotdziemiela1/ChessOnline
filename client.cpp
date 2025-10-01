#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib") 

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 64
#define CLIENT_BUF_SIZE 64

// Chess board is 8x8 tiles

int main(int argc, char* argv[]){ // Don't pass any aruguments if you want to connect to localhost
    printf("argument passed: %s", argv[1]);

    // WSA startup
    WSADATA wsaData;

    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0){
        printf("WSAStartup error: %d\n", iResult);
    }

    // getting select ports
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0){
        printf("getaddrinfo() error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Creating Socket
    SOCKET connectSocket = INVALID_SOCKET;

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next){
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET){
            printf("socket() error: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server
        iResult = connect(connectSocket, ptr->ai_addr, ptr->ai_addrlen);
        if (iResult == 0) {
            break;
        } else {
            printf("connect() error: %d\n", iResult);
            closesocket(connectSocket);
            connectSocket = INVALID_SOCKET;
        } 
    }
    

    freeaddrinfo(result);

    if (connectSocket == INVALID_SOCKET){
        printf("Unable to connect to server.\n");
        WSACleanup();
        return 1;
    }

    
    // shutdown sending portion of socket
    // TODO This is just for current testing
    // iResult = shutdown(connectSocket, SD_SEND);
    // if (iResult == SOCKET_ERROR) {
        //     printf("shutdown sending error: %d\n", WSAGetLastError());
        //     closesocket(connectSocket);
        //     WSACleanup();
        //     return 1;
        // }
        
    // read input string from stdin
    char sendbuf[CLIENT_BUF_SIZE];
    // recieve data from server
    char recvbuf[DEFAULT_BUFLEN];
    do {
        printf("Input string to send to server: ");
        fgets(sendbuf, sizeof(sendbuf), stdin);
        
        // send data to server
        iResult = send(connectSocket, sendbuf, (int)sizeof(sendbuf), 0);
        if (iResult == SOCKET_ERROR){
            printf("send() error: %d\n", WSAGetLastError());
            closesocket(connectSocket);
            WSACleanup();
            return -1;
        }

        iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0){
            printf("bytes received: %d\n", iResult);
            printf("string received: %s\n", recvbuf);
        } else if (iResult == 0) {
            printf("connection to server has closed");
        } else {
            printf("recv error: %d\n", WSAGetLastError());
        }
    } while (iResult > 0);

    closesocket(connectSocket);
    WSACleanup();

    return 0;
}