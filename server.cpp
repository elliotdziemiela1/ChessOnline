#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib") 

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

int main(){
    // WSA startup
    WSADATA wsaData;

    int startupResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (startupResult != 0){
        printf("WSAStartup error: %d\n", startupResult);
    }

    // getting addresses and ports to be used

    struct addrinfo *result = NULL, *ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0){
        printf("getaddrinfo error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // creating socket

    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET){
        printf("socket() error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // binding socket
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR){
        printf("bind() error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // result is no longer needed so it must be freed
    freeaddrinfo(result);

    // listening on socket
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR){
        printf("listen() error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Accepting a connection
    SOCKET clientSocket = INVALID_SOCKET;
    clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET){
        printf("accept() error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Don't need server socket once connection is accepted
    closesocket(listenSocket); // TODO remove this if having two players

    // receive and send data on socket
    char recvbuf[DEFAULT_BUFLEN];
    int iResult, iSendResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Receive until the peer shuts down the connection (iResult == 0)
    do {

        iResult = recv(clientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);

            // Echo the buffer back to the sender
            iSendResult = send(clientSocket, recvbuf, iResult, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send() error: %d\n", WSAGetLastError());
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }
            printf("Bytes sent: %d\n", iSendResult);
        } else if (iResult == 0)
            printf("Connection closing...\n");
        else {
            printf("recv() error: %d\n", WSAGetLastError());
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

    } while (iResult > 0);

    // connection closed (iResult == 0)
    iResult = shutdown(clientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR){
        printf("shutdown() error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
    }

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}