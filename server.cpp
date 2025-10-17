#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <cstring>
#include <string>
#pragma comment(lib, "ws2_32.lib") 

#include "utils.h"
#include "game.h"

// The game logic itself is located in game.cpp

// This simply closes the sockets passed as arguments and calls WSACleanup.
void cleanup(SOCKET s1, SOCKET s2) {
    closesocket(s1);
    if (s2 != INVALID_SOCKET){
        closesocket(s2);
    }
    WSACleanup();
    return;
}

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

    printf("Waiting to receive connection from client.\n");

    // listening on socket
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR){
        printf("listen() error: %d\n", WSAGetLastError());
        cleanup(listenSocket, INVALID_SOCKET);
        return 1;
    }

    // Accepting a connection
    SOCKET clientSocketOne = INVALID_SOCKET;
    clientSocketOne = accept(listenSocket, NULL, NULL);
    if (clientSocketOne == INVALID_SOCKET){
        printf("accept() first client error: %d\n", WSAGetLastError());
        cleanup(listenSocket, INVALID_SOCKET);
        return 1;
    }
    printf("Client one connected, waiting for client two.\n");

    const char *sendbuf;

    // $ is the delimiter for the end of the message. $R tells client to wait to receive another message.
    // $S tells client to send a message.
    sendbuf = "Welcome to Chess Online, Player 1! Server is waiting for player 2 to connect.\n\
Controls: input the tile of the piece you would like to move first, followed directly by the destination tile. \
Example: c1e3 would attempt to move the piece at c1 to position e3.$R";
    if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Welcome message to client one error: %d", WSAGetLastError());
        cleanup(clientSocketOne, listenSocket);
        return 1;
    }

    // Accepting another connection
    SOCKET clientSocketTwo = INVALID_SOCKET;
    clientSocketTwo = accept(listenSocket, NULL, NULL);
    if (clientSocketTwo == INVALID_SOCKET){
        printf("accept() second client error: %d\n", WSAGetLastError());
        cleanup(listenSocket, INVALID_SOCKET);
        return 1;
    }
    printf("Client two connected.");

    // Don't need server socket once two connections are accepted
    closesocket(listenSocket); 

    sendbuf = "Welcome to Chess Online, Player 2! Player 1 will start as White.\n\
Controls: input the tile of the piece you would like to move first, followed directly by the destination tile. \
Example: c1e3 would attempt to move the piece at c1 to position e3.$R";
    if (send(clientSocketTwo, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Welcome message to client two error: %d", WSAGetLastError());
        cleanup(clientSocketOne, clientSocketTwo);
        return 1;
    }

    Game game; // Instatiate the game


    // receive and send data on socket
    char recvbuf[DEFAULT_BUFLEN];
    // recvbuf[DEFAULT_BUFLEN] = '\0'; // end with null for printing purposes. when calling recv function, effective buffer size is DEFAULT_BUFLEN
    int iSendResult;

    // setting up buffer that holds the printed table. This table is of fixed size and will have it's contents replaced after every move.
    char tablebuf[DEFAULT_BUFLEN];
    tablebuf[PRINTED_BOARD_SIZE] = '$';
    tablebuf[PRINTED_BOARD_SIZE+1] = 'R';
    tablebuf[PRINTED_BOARD_SIZE+2] = '\0';

    // Sending table to client 1
    game.format_table_to_print(tablebuf); // update the contents of the printed table buffer
    tablebuf[PRINTED_BOARD_SIZE+1] = 'R'; // tell client 1 to recieve a message from server after recieving table

    // Send the printed table
    if (send(clientSocketOne, tablebuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Sending table to client 1 error: %d", WSAGetLastError());
        cleanup(clientSocketOne, clientSocketTwo);
        return 1;
    }

    sendbuf = "Player two has connected. It's your turn to make the first move as White.\
$S";
    if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Third welcome message to client one error: %d", WSAGetLastError());
        cleanup(clientSocketOne, clientSocketTwo);
        return 1;
    }

    do {
        
        ///////////////////////////////
        /// Client 1 (White) move ////
        /////////////////////////////
        printf("Waiting for client 1 to make move.\n");
        if (recv(clientSocketOne,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
            printf("Client 1 receive error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }
        printf("client 1's move: %s", recvbuf);

        while(!game.make_move(recvbuf, 'W')){ // if condition is false, move was invalid and server requests another
            sendbuf = "Invalid move. Try again:$S";
            if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
                printf("client 1 send error: %d\n", WSAGetLastError());
                cleanup(clientSocketOne, clientSocketTwo);
                return 1;
            }
            printf("Waiting for client 1 to make move.\n");
            if (recv(clientSocketOne,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
                printf("Client 1 receive error: %d\n", WSAGetLastError());
                cleanup(clientSocketOne, clientSocketTwo);
                return 1;
            }
            printf("client 1's move: %s", recvbuf);
        } 

        // Send table again to show client 1 where they moved
        game.format_table_to_print(tablebuf); // update the contents of the printed table buffer
        tablebuf[PRINTED_BOARD_SIZE+1] = 'R'; // tell client 1 to wait for message from server after recieving table
        if (send(clientSocketOne, tablebuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("Sending table pt.2 to client 1 error: %d", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Now sending confirmation to client 1 and telling them to wait for another message.
        sendbuf = "Nice move. Now waiting for Black's move.$R";
        if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("client 1 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // sending updated table to client 2
        tablebuf[PRINTED_BOARD_SIZE+1] = 'R'; // tell client 2 to wait for message from server after recieving table
        if (send(clientSocketTwo, tablebuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("Sending table pt.1 to client 2 error: %d", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Telling client 2 of the move client 1 just made, and telling it to send a message
        std::string c1move(recvbuf);
        std::string msg("White just moved: ");
        msg = msg+c1move+"Your turn now: $S";
        if (send(clientSocketTwo, msg.c_str(),(int)msg.length(),0) == SOCKET_ERROR){
            printf("Client 2 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        //////////////////////////////
        /// Client 2 (Black) move ////
        //////////////////////////////
        printf("Waiting for client 2 to make move.\n");
        if (recv(clientSocketTwo,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
            printf("Client 2 receive error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }
        printf("client 2's move: %s", recvbuf);

        while(!game.make_move(recvbuf, 'B')){ // if condition is false, move was invalid and server requests another
            sendbuf = "Invalid move. Try again:$S";
            if (send(clientSocketTwo, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
                printf("client 2 send error: %d\n", WSAGetLastError());
                cleanup(clientSocketOne, clientSocketTwo);
                return 1;
            }
            printf("Waiting for client 2 to make move.\n");
            if (recv(clientSocketTwo,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
                printf("Client 1 receive error: %d\n", WSAGetLastError());
                cleanup(clientSocketOne, clientSocketTwo);
                return 1;
            }
            printf("client 2's move: %s", recvbuf);
        } 

        // Send table again to show client 2 where they moved
        game.format_table_to_print(tablebuf); // update the contents of the printed table buffer
        tablebuf[PRINTED_BOARD_SIZE+1] = 'R'; // tell client 2 to wait for message from server after recieving table
        if (send(clientSocketTwo, tablebuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("Sending table pt.2 to client 2 error: %d", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Now sending confirmation to client 2 and telling them to wait for another message.
        sendbuf = "Nice move. Now waiting for White's move.$R";
        if (send(clientSocketTwo, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("client 2 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Sending updated table to client 1
        game.format_table_to_print(tablebuf); // update the contents of the printed table buffer
        tablebuf[PRINTED_BOARD_SIZE+1] = 'R'; // tell client 1 to send a move to server after recieving table
        // Send the printed table
        if (send(clientSocketOne, tablebuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("Sending table pt.1 to client 1 error: %d", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Telling client 1 of the move client 2 just made
        std::string c2move(recvbuf);
        std::string msg2("Black just moved: ");
        msg2 = msg2+c2move+"Your turn now: $S";
        if (send(clientSocketOne, msg2.c_str(),(int)msg2.length(),0) == SOCKET_ERROR){
            printf("Client 1 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

    } while (!game.get_white_in_checkmate() && !game.get_black_in_checkmate());

    // connection closed (iResult == 0)
    iResult = shutdown(clientSocketOne, SD_SEND);
    if (iResult == SOCKET_ERROR){
        printf("shutdown() client one error: %d\n", WSAGetLastError());
        // closesocket(clientSocketOne);
        // closesocket(clientSocketTwo);
        // WSACleanup();
    }
    iResult = shutdown(clientSocketTwo, SD_SEND);
    if (iResult == SOCKET_ERROR){
        printf("shutdown() client two error: %d\n", WSAGetLastError());
    }

    cleanup(clientSocketOne, clientSocketTwo);

    return 0;
}