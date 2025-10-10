#include "game.h"

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
//      (i.e. "a2", "d5") which get sent to the server. Then, steps 1-6 following this will fall under a single function in the game class
//      called by the server which is named "is_move_valid".
//      If that function returns false, the client is prompted to try again. If it returns true, steps 7 and 8 are executed
//  2) These coordinates are format checked. If format is invalid, don't allow the move. Else, convert to integer coordiantes (i.e. "a2" -> 12, "d5" -> 45).
//      Then the server will convert these integers into pair<int,int>. 
//  3) If there is a piece at the starting position that is of the current player's color, the type of the piece at the starting coordiante will be 
//      saved as a local variable serverside, and so will the
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
//          6aiv) Move to step 7
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
//          rook or king, set the moved flag for that piece. If castle-ing, move both the king and the corresponding rook accordingly.
//      8b) If it's occupied by an enemy piece, go into board structure and replace empty character with the selected piece's representation.
//          Then add opponent piece to dead list for that color. If client is moving a rook, set the moved flag for that rook.
//      8c) If the piece being moved is a pawn and it reaches the other end of the board, prompt the client to choose a piece of theirs to
//              ressurect. The dead list for their color (can look at local variable for color saved in step 3) will be displayed
//              and they will type in a target for ressurection. replace destination tile with ressurected piece. 
//  

Game::Game() : WR1_moved(false), WR2_moved(false), WK_moved(false), BR1_moved(false), BR2_moved(false), BK_moved(false),
white_in_checkmate(false), black_in_checkmate(false){

};


bool Game::get_white_in_checkmate(){
    return this->white_in_checkmate;
}

bool Game::get_black_in_checkmate(){
    return this->black_in_checkmate;
}

bool Game::is_move_valid(char buf[DEFAULT_BUFLEN]){
    return true;
};