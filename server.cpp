#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <cstring>
#include <string>
#pragma comment(lib, "ws2_32.lib") 

#include "utils.h"

// chess board is 8x8 tiles. White is always on bottom, and black is always on top.
// server will keep track of entire board with a 2d vector of strings. Each of these strings will have a character
// for the color, a character for the piece type, and a number character for the specific knight it is (needed for castle-ing),
// if it is a knight (i.e. BK = black king,  WN1 = white knight one). 

// There will also be a dead list for white, and a dead list for black, keeping track of what pieces have been removed from the board.

// There will be six flags: WK_moved, BK_moved, WR1_moved, WR2_moved, BR1_moved, BR2_moved. These let us know if certain rooks or kings
// have moved already for castle-ing.

// A king is the peice that initiates a castle. Simply move the king two spaces to the right or left of where it started to perform a castle.

// Clients will only receive renderings of the board.

//////////////////////////////////////
// A step by step rundown of a turn:
//////////////////////////////////////

//  1) When a client takes a turn, they will type the starting position coordinate and the ending position coordinate
//      (i.e. "a2", "d5") which get converted to integer coordiantes clientside and sent to the
//      server (i.ei "a2" -> 12, "d5" -> 45).
//  2) The server will convert these integers into pair<int,int>. 
//  3) If there is a piece at the starting position, the type of the piece at the starting coordiante will be saved as a local variable serverside, and so will the
//      starting position and the color of the piece. Otherwise, the move won't be accepted.
//  4) The difference vector (as a pair<int,int>) will be calculated (i.e. for a bishop moving up and right, 4-1,5-2 = (3,3)),
//      or (i.e. for a queen moving left, 1-6,2-2 = (-5,0)).
//  5) The piece type will be used to index into a hash table (map) where the keys are piece types and 
//      the values are vector<pair<int,int>> containing every possible movement vector for that piece. If no match is found
//      in that vector for the movement difference vector we just calculated, the server sends "move invalid" and waits for 
//      that client to submit a new move (go back to step 1).
//  6) If a match is found, the next steps depend on the piece type.
//      6a) if the piece is a pawn
//          6ai) if the movment is forward by one ((0,1) or (0,-1)) and there's a piece in the way, don't allow the move.
//          6aii) if the movement is forward by two ((0,2) or (0,-2)) and there's a piece in the way, don't allow the move.
//                  Or if the pawn isn't in row 2 with vector (0,2), dont allow the move. Or if the pawn isn't
//                  in row 6 with vector (0,-2), don't allow the move.
//          6aiii) If the pawn is white if the movement is diagonal by one ((1,1), (-1,1)) and there isn't a piece to capture,
//                  don't allow the move. Same for if the pawn is black and the movement is down diagonal by one and there isn't
//                  a piece to capture, don't allow the move.
//          6aiv) If pawn move is valid and it reaches the other end of the board, prompt the client to choose a piece of theirs to
//              ressurect. The dead list for their color (can look at local variable for color saved in step 3) will be displayed
//              and they will type in a target for ressurection. Then replace starting tile with empty character and replace destination
//              tile with ressurected piece. The turn is now over.
//          6av) Move to step 7
//      6b) if the piece is a bishop or queen
//          6bi) if theres a piece in the way (add difference vector to starting position tile by tile and check) don't allow move.
//          6bii) Move to step 7
//      6c) if the piece is a rook
//          6ci) if theres a piece in the way (add difference vector to starting position tile by tile and check) don't allow move.
//          6cii) Move to step 7
//      6d) if the piece is a king
//          6di) if client wants to move king two spaces to the right, if either the rook2 moved or king moved flags are set, or
//               if there's pieces in the way between king and rook2, don't allow move. 
//          6dii) if client wants to move king two spaces to the left, if either the rook1 moved or king moved flags are set, or
//               if there's pieces in the way between king and rook1, don't allow move. 
//          6diii) Move to step 7
//      6e) if the piece is a knight
//          6ei) Always allow move since knight can jump over pieces.
//          6eii) Move to step 7
//  7) Replace starting tile with empty character in board structure (underscore maybe)
//  8) Check destination itself. 
//      8a) If it's empty, go into board structure and replace empty character with the selected piece's representation. If client is moving a 
//          rook or king, set the moved flag for that rook. If castle-ing, move both the king and the corresponding rook accordingly.
//      8a) If it's occupied by an enemy piece, go into board structure and replace empty character with the selected piece's representation.
//          Then add opponent piece to dead list for that color. If client is moving a rook, set the moved flag for that rook.
//  

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
    sendbuf = "Welcome to Chess Online, Player 1! Server is waiting for player 2 to connect.$R";
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

    sendbuf = "Welcome to Chess Online, Player 2! Player 1 will start as White.$R";
    if (send(clientSocketTwo, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Welcome message to client two error: %d", WSAGetLastError());
        cleanup(clientSocketOne, clientSocketTwo);
        return 1;
    }


    // receive and send data on socket
    char recvbuf[DEFAULT_BUFLEN];
    // recvbuf[DEFAULT_BUFLEN] = '\0'; // end with null for printing purposes. when calling recv function, effective buffer size is DEFAULT_BUFLEN
    int iSendResult;

    sendbuf = "Player two has connected. It's your turn to make the first move as White.\
$S";
    if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
        printf("Second welcome message to client one error: %d", WSAGetLastError());
        cleanup(clientSocketOne, clientSocketTwo);
        return 1;
    }

    bool checkmate = false;
    do {
        //////////////////////
        /// Client 1 move ////
        //////////////////////
        printf("Waiting for client 1 to make move.\n");
        if (recv(clientSocketOne,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
            printf("Client 1 receive error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }
        printf("client 1's move: %s", recvbuf);
        // Now sending confirmation to client 1 and telling them to wait for another message.
        sendbuf = "Nice move. Now waiting for Black's move.$R";
        if (send(clientSocketOne, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("client 1 send error: %d\n", WSAGetLastError());
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

        //////////////////////
        /// Client 2 move ////
        //////////////////////
        printf("Waiting for client 2 to make move.\n");
        if (recv(clientSocketTwo,recvbuf,DEFAULT_BUFLEN,0) == SOCKET_ERROR){
            printf("Client 2 receive error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }
        printf("client 2's move: %s", recvbuf);
        // Now sending confirmation to client 2 and telling them to wait for another message.
        sendbuf = "Nice move. Now waiting for White's move.$R";
        if (send(clientSocketTwo, sendbuf, DEFAULT_BUFLEN, 0) == SOCKET_ERROR){
            printf("client 2 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

        // Telling client 1 of the move client 2 just made, and telling it to send a message
        std::string c2move(recvbuf);
        std::string msg2("Black just moved: ");
        msg2 = msg2+c2move+"Your turn now: $S";
        if (send(clientSocketOne, msg2.c_str(),(int)msg2.length(),0) == SOCKET_ERROR){
            printf("Client 1 send error: %d\n", WSAGetLastError());
            cleanup(clientSocketOne, clientSocketTwo);
            return 1;
        }

    } while (!checkmate);

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