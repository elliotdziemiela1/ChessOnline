#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#pragma comment(lib, "ws2_32.lib") 

#include "utils.h"

// Chess board is 8x8 tiles

int main(int argc, char* argv[]){ // Don't pass any aruguments if you want to connect to localhost
    // printf("argument passed: %s\n", argv[1]);

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
            // printf("connect() error: %d\n", iResult);
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

        
    // read input string from stdin
    char sendbuf[DEFAULT_BUFLEN];

    // recieve data from server
    char recvbuf[DEFAULT_BUFLEN];

    // loop that recieves data. That data that the server sends will have a delimiter (null char). After
    // the delimiter, there will be an indication as to whether the client should expect to recieve more data ('R')
    // or if the client should send data ('S')
    char next_step = 'R';
    do {
        if (next_step == 'R'){
            iResult = recv(connectSocket, recvbuf, DEFAULT_BUFLEN, 0);
            if (iResult > 0){
                std::string s(recvbuf);
                int idx = s.find('$'); // get index of delimiter
                std::string subs = s.substr(0,idx);
                printf("%s\n", subs.c_str()); // print up until delimiter
                next_step = s.at(idx+1); // Get either an 'S' for send or an 'R' for receive after the delimiter.
            } else if (iResult == 0){
                printf("Connection to server closed.\n");
            } else {
                printf("Error with receiving data from server: %d\n", WSAGetLastError());
            }
        } else if (next_step == 'S'){
            fgets(sendbuf, DEFAULT_BUFLEN, stdin);
            iResult = send(connectSocket, sendbuf, DEFAULT_BUFLEN, 0);
            if (iResult == SOCKET_ERROR){
                printf("send() error: %d\n", WSAGetLastError());
                break;
            }
            next_step = 'R';
        } else if (next_step == 'E'){
            printf("Server is ending the game.");
            break;
        }
    } while (iResult > 0);

    closesocket(connectSocket);
    WSACleanup();

    return 0;
}