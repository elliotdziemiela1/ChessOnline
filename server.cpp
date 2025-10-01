#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib") 

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 64

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
    // recvbuf[DEFAULT_BUFLEN] = '\0'; // end with null for printing purposes. when calling recv function, effective buffer size is DEFAULT_BUFLEN
    int iSendResult;

    // Receive until the peer shuts down the connection (iResult == 0)
    do {
        printf("Waiting to receive data from client.\n");
        iResult = recv(clientSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
            printf("string received: %s\n", recvbuf);

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