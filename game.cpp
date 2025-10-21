#include <utility>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <cmath>

#include "game.h"
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
//  5) The movement difference vector is examined based on the piece type to determine validity. If the move is deemed invalid, don't allow it.
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
    white_won(false), black_won(false), replace_pawn_flag(false){
    // All moves on the board will be in the format (row, col)
    table = {
            {"BR1", "BN", "BB", "BQ", "BK", "BB", "BN", "BR2"},
            {"BP" , "BP", "BP", "BP", "BP", "BP", "BP", "BP" },
            {"  " , "  ", "  ", "  ", "  ", "  ", "  ", "  " },
            {"  " , "  ", "  ", "  ", "  ", "  ", "  ", "  " },
            {"  " , "  ", "  ", "  ", "  ", "  ", "  ", "  " },
            {"  " , "  ", "  ", "  ", "  ", "  ", "  ", "  " },
            {"WP" , "WP", "WP", "WP", "WP", "WP", "WP", "WP" },
            {"WR1", "WN", "WB", "WQ", "WK", "WB", "WN", "WR2"}
        };

    knight_move_vectors = {
        {2,1},{1,2},{-1,2},{-2,1},{-2,-1},{-1,-2},{1,-2},{2,-1}
    };

    white_dead_list = {};
    black_dead_list = {};
};

bool Game::get_black_won(){
    return this->black_won;
}

bool Game::get_white_won(){
    return this->white_won;
}

// writes the table in a pretty format to the given buffer.
void Game::format_table_to_print(char buf[DEFAULT_BUFLEN]){
    std::vector<std::vector<char>> pretty_table = {
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'8',' ','|',' ',table[0][0].at(0),table[0][0].at(1),' ','|',' ',table[0][1].at(0),table[0][1].at(1),' ','|',' ',table[0][2].at(0),table[0][2].at(1),' ','|',' ',table[0][3].at(0),table[0][3].at(1),' ','|',' ',table[0][4].at(0),table[0][4].at(1),' ','|',' ',table[0][5].at(0),table[0][5].at(1),' ','|',' ',table[0][6].at(0),table[0][6].at(1),' ','|',' ',table[0][7].at(0),table[0][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'7',' ','|',' ',table[1][0].at(0),table[1][0].at(1),' ','|',' ',table[1][1].at(0),table[1][1].at(1),' ','|',' ',table[1][2].at(0),table[1][2].at(1),' ','|',' ',table[1][3].at(0),table[1][3].at(1),' ','|',' ',table[1][4].at(0),table[1][4].at(1),' ','|',' ',table[1][5].at(0),table[1][5].at(1),' ','|',' ',table[1][6].at(0),table[1][6].at(1),' ','|',' ',table[1][7].at(0),table[1][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'6',' ','|',' ',table[2][0].at(0),table[2][0].at(1),' ','|',' ',table[2][1].at(0),table[2][1].at(1),' ','|',' ',table[2][2].at(0),table[2][2].at(1),' ','|',' ',table[2][3].at(0),table[2][3].at(1),' ','|',' ',table[2][4].at(0),table[2][4].at(1),' ','|',' ',table[2][5].at(0),table[2][5].at(1),' ','|',' ',table[2][6].at(0),table[2][6].at(1),' ','|',' ',table[2][7].at(0),table[2][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'5',' ','|',' ',table[3][0].at(0),table[3][0].at(1),' ','|',' ',table[3][1].at(0),table[3][1].at(1),' ','|',' ',table[3][2].at(0),table[3][2].at(1),' ','|',' ',table[3][3].at(0),table[3][3].at(1),' ','|',' ',table[3][4].at(0),table[3][4].at(1),' ','|',' ',table[3][5].at(0),table[3][5].at(1),' ','|',' ',table[3][6].at(0),table[3][6].at(1),' ','|',' ',table[3][7].at(0),table[3][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'4',' ','|',' ',table[4][0].at(0),table[4][0].at(1),' ','|',' ',table[4][1].at(0),table[4][1].at(1),' ','|',' ',table[4][2].at(0),table[4][2].at(1),' ','|',' ',table[4][3].at(0),table[4][3].at(1),' ','|',' ',table[4][4].at(0),table[4][4].at(1),' ','|',' ',table[4][5].at(0),table[4][5].at(1),' ','|',' ',table[4][6].at(0),table[4][6].at(1),' ','|',' ',table[4][7].at(0),table[4][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'3',' ','|',' ',table[5][0].at(0),table[5][0].at(1),' ','|',' ',table[5][1].at(0),table[5][1].at(1),' ','|',' ',table[5][2].at(0),table[5][2].at(1),' ','|',' ',table[5][3].at(0),table[5][3].at(1),' ','|',' ',table[5][4].at(0),table[5][4].at(1),' ','|',' ',table[5][5].at(0),table[5][5].at(1),' ','|',' ',table[5][6].at(0),table[5][6].at(1),' ','|',' ',table[5][7].at(0),table[5][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'2',' ','|',' ',table[6][0].at(0),table[6][0].at(1),' ','|',' ',table[6][1].at(0),table[6][1].at(1),' ','|',' ',table[6][2].at(0),table[6][2].at(1),' ','|',' ',table[6][3].at(0),table[6][3].at(1),' ','|',' ',table[6][4].at(0),table[6][4].at(1),' ','|',' ',table[6][5].at(0),table[6][5].at(1),' ','|',' ',table[6][6].at(0),table[6][6].at(1),' ','|',' ',table[6][7].at(0),table[6][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {'1',' ','|',' ',table[7][0].at(0),table[7][0].at(1),' ','|',' ',table[7][1].at(0),table[7][1].at(1),' ','|',' ',table[7][2].at(0),table[7][2].at(1),' ','|',' ',table[7][3].at(0),table[7][3].at(1),' ','|',' ',table[7][4].at(0),table[7][4].at(1),' ','|',' ',table[7][5].at(0),table[7][5].at(1),' ','|',' ',table[7][6].at(0),table[7][6].at(1),' ','|',' ',table[7][7].at(0),table[7][7].at(1),' ','|','\n'},
        {' ',' ','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','-','-','-','-','+','\n'},
        {' ',' ',' ',' ','a',' ',' ',' ',' ','b',' ',' ',' ',' ','c',' ',' ',' ',' ','d',' ',' ',' ',' ','e',' ',' ',' ',' ','f',' ',' ',' ',' ','g',' ',' ',' ',' ','h',' ',' ',' ','\n'}
    };

    for (int i = 0; i < PRINTED_BOARD_ROWS; i++){
        for (int j = 0; j < PRINTED_BOARD_COLS; j++){
            buf[i*PRINTED_BOARD_COLS+j] = pretty_table[i][j];
        }
    }
}


bool Game::make_move(char buf[DEFAULT_BUFLEN], char player_color){
    bool attempting_left_castle; // this will refer to castles on the left side of the board, i.e. BK and BR1, or WK and WR1
    bool attempting_right_castle; // this will refer to castles on the right side of the board, i.e. BK and BR2, or WK and WR2
    std::string piece; // the piece being moved
    std::string end_piece; // the piece (or blank space) at the end coordinate

    //////////////////////////////////////
    //// Checking input /////
    //////////////////////////////////////
    if (buf[0] < 'a' || buf[0] > 'h') return false;
    if (buf[1] < '1' || buf[1] > '8') return false;
    if (buf[2] < 'a' || buf[2] > 'h') return false;
    if (buf[3] < '1' || buf[3] > '8') return false;
    if (buf[4] != '\n' && buf[5] != '\0') return false;

    //////////////////////////////////////
    //// Reformatting /////
    //////////////////////////////////////
    // Turn starting and ending coordiantes into integer coordinates that are swapped for (row,col) table indexing i.e. (a,5) -> (5,1)
    std::pair<int,int> start_coord(7 - ((int)buf[1] - (int)('1')),((int)buf[0] - (int)('a')));
    std::pair<int,int> end_coord(7 - ((int)buf[3] - (int)('1')),((int)buf[2] - (int)('a')));


    //////////////////////////////////////
    //// Getting pieces from table /////
    //////////////////////////////////////
    piece = table[start_coord.first][start_coord.second];
    end_piece = table[end_coord.first][end_coord.second];

    // sanity check: piece being moved is the same as the current player's piece
    if (player_color != piece.at(0))
        return false;


    //////////////////////////////////////
    //// Creating move vector /////
    //////////////////////////////////////
    std::pair<int,int> move_vector = {end_coord.first-start_coord.first, end_coord.second-start_coord.second};

    // sanity check: if not moving at all, return false
    if (move_vector.first == 0 && move_vector.second == 0) 
        return false;

    // sanity check: if friendly piece at endcoord, return false
    if (piece.at(0) == end_piece.at(0))
        return false;
    
    //////////////////////////////////////
    //// Handling rook movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'R'){
        if (move_vector.first != 0 && move_vector.second != 0) // either row-movement or col-movement must be 0 for a rook
            return false;

        // vector is valid, now check for collisions in the path leading up to end_coord
        std::pair<int,int> iterative_coord = start_coord;
        do {
            // increment the iterative coordinate based on direction of move_vector
            if (move_vector.first > 0)
                iterative_coord.first++;
            else if (move_vector.first < 0)
                iterative_coord.first--;
            else if (move_vector.second > 0)
                iterative_coord.second++;
            else if (move_vector.second < 0)
                iterative_coord.second--;
            // if a piece is in the way
            if (iterative_coord != end_coord && table[iterative_coord.first][iterative_coord.second] != "  ")
                return false;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W')
            white_dead_list.push_back(end_piece);
        if (end_piece.at(0) == 'B')
            black_dead_list.push_back(end_piece);

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        // set rook moved flag
        if (piece == "WR1")
            WR1_moved = true;
        else if (piece == "WR2")
            WR2_moved = true;
        else if (piece == "BR1")
            BR1_moved = true;
        else if (piece == "BR2")
            BR2_moved = true;

        return true;
    } 

    //////////////////////////////////////
    //// Handling knight movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'N'){
        bool valid_flag = false;
        for (std::pair<int,int> p : knight_move_vectors){ // loop through all the possible knight movement vectors
            if (p == move_vector){
                valid_flag = true;
                break;
            }
        }
        if (!valid_flag)
            return false;

        // check if we're removing a piece
        if (end_piece.at(0) == 'W')
            white_dead_list.push_back(end_piece);
        if (end_piece.at(0) == 'B')
            black_dead_list.push_back(end_piece);

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;
        
        return true;
    } 

    //////////////////////////////////////
    //// Handling bishop movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'B'){
        if (std::abs(move_vector.first) != std::abs(move_vector.second)) // for diagonal movement, row-movement and col-movement must be equal
            return false;
        
        // vector is valid, now check for collisions in the path leading up to end_coord
        std::pair<int,int> iterative_coord = start_coord;
        do {
            // increment the iterative coordinate based on direction of move_vector
            if (move_vector.first > 0)
                iterative_coord.first++;
            else if (move_vector.first < 0)
                iterative_coord.first--;
            
            if (move_vector.second > 0)
                iterative_coord.second++;
            else if (move_vector.second < 0)
                iterative_coord.second--;

            // if a piece is in the way
            if (iterative_coord != end_coord && table[iterative_coord.first][iterative_coord.second] != "  ")
                return false;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W')
            white_dead_list.push_back(end_piece);
        if (end_piece.at(0) == 'B')
            black_dead_list.push_back(end_piece);

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        return true;
    } 
    
    //////////////////////////////////////
    //// Handling queen movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'Q'){
        if (move_vector.first != 0 && move_vector.second != 0){ // if not straight line movement
            if (std::abs(move_vector.first) != std::abs(move_vector.second)) // if not diagonal movement
                return false;
        }

        // vector is valid, now check for collisions in the path leading up to end_coord
        std::pair<int,int> iterative_coord = start_coord;
        do {
            // increment the iterative coordinate based on direction of move_vector
            if (move_vector.first > 0)
                iterative_coord.first++;
            else if (move_vector.first < 0)
                iterative_coord.first--;
            
            if (move_vector.second > 0)
                iterative_coord.second++;
            else if (move_vector.second < 0)
                iterative_coord.second--;

            // if a piece is in the way
            if (iterative_coord != end_coord && table[iterative_coord.first][iterative_coord.second] != "  ")
                return false;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W')
            white_dead_list.push_back(end_piece);
        if (end_piece.at(0) == 'B')
            black_dead_list.push_back(end_piece);

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        return true;
    } 
    
    //////////////////////////////////////
    //// Handling king movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'K'){
        if (move_vector.second==-2 && move_vector.first==0){ // first we check for left castleing
            if (piece.at(0)=='W' && !WK_moved  && !WR1_moved){ // attempting white left castle
                // check for collisions between the white king and the left rook
                if (table[7][3] != "  " || table[7][2] != "  " || table[7][1] != "  ")
                    return false;
                // make move
                table[7][2] = "WK";
                table[7][3] = "WR1";
                table[7][4] = "  ";
                table[7][0] = "  ";
                WK_moved = true;
                return true;
            } else if (piece.at(0)=='B' && !BK_moved  && !BR1_moved){ // attempting black left castle
                // check for collisions between the black king and the left rook
                if (table[0][3] != "  " || table[0][2] != "  " || table[0][1] != "  ")
                    return false;
                // make move
                table[0][2] = "BK";
                table[0][3] = "BR1";
                table[0][4] = "  ";
                table[0][0] = "  ";
                BK_moved = true;
                return true;
            }
        } else if (move_vector.second==2 && move_vector.first==0){ // then we check for right castleing
            if (piece.at(0)=='W' && !WK_moved  && !WR2_moved){ // attempting white right castle
                // check for collisions between the white king and the right rook
                if (table[7][5] != "  " || table[7][6] != "  ")
                    return false;
                // make move
                table[7][6] = "WK";
                table[7][5] = "WR2";
                table[7][4] = "  ";
                table[7][7] = "  ";
                WK_moved = true;
                return true;
            } else if (piece.at(0)=='B' && !BK_moved  && !BR2_moved){ // attempting black right castle
                // check for collisions between the black king and the right rook
                if (table[0][5] != "  " || table[0][6] != "  ")
                    return false;
                // make move
                table[0][5] = "BK";
                table[0][6] = "BR2";
                table[0][4] = "  ";
                table[0][7] = "  ";
                BK_moved = true;
                return true;
            }
        } else if (std::abs(move_vector.first) > 1 || std::abs(move_vector.second) > 1){ // If not castleing, we check
// that king is only moving 1 tile away
            return false;
        }

        // if removing a piece, add it to dead list
        if (end_piece.at(0) == 'W')
            white_dead_list.push_back(end_piece);
        else if (end_piece.at(0) == 'B')
            black_dead_list.push_back(end_piece);
        

        // set king moved flag
        if (piece.at(0) == 'W')
            WK_moved = true;
        else if (piece.at(0) == 'B')
            BK_moved = true;


        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        return true;
    } 
    
    //////////////////////////////////////
    //// Handling pawn movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'P'){
        // if player wants to move pawn two spaces
        if (move_vector.first == 2 && move_vector.second == 0){ // 2 is movement towards white side
            // check that pawn is of color and position to perform such a move
            if (!(piece.at(0) == 'B' && start_coord.first == 1))
                return false;

            // check if there's a collision
            if (table[start_coord.first+1][start_coord.second] != "  ")
                return false;
        } else if (move_vector.first == -2  && move_vector.second == 0){ // -2 is movement towards black side
            // check that pawn is of color and position to perform such a move
            if (!(piece.at(0) == 'W' && start_coord.first == 6))
                return false;

            // check if there's a collision
            if (table[start_coord.first-1][start_coord.second] != "  ")
                return false;
        } else if (move_vector.first == 1 && move_vector.second == 1){ // movement down and right
            // check that pawn is black
            if (piece.at(0) != 'B')
                return false;

            // piece at end_coordinate must be white
            if (end_piece.at(0) == 'W')
                white_dead_list.push_back(end_piece);
            else 
                return false;

        } else if (move_vector.first == 1 && move_vector.second == -1){ // movement down and left
            // check that pawn is black
            if (piece.at(0) != 'B')
                return false;

            // piece at end_coordinate must be white
            if (end_piece.at(0) == 'W')
                white_dead_list.push_back(end_piece);
            else 
                return false;

        } else if (move_vector.first == -1 && move_vector.second == 1){ // movement up and right
            // check that pawn is white
            if (piece.at(0) != 'W')
                return false;

            // piece at end_coordinate must be black
            if (end_piece.at(0) == 'B')
                black_dead_list.push_back(end_piece);
            else 
                return false;

        } else if (move_vector.first == -1 && move_vector.second == -1){ // movement up and left
            // check that pawn is white
            if (piece.at(0) != 'W')
                return false;

            // piece at end_coordinate must be black
            if (end_piece.at(0) == 'B')
                black_dead_list.push_back(end_piece);
            else 
                return false;
        } else if (move_vector.first == 1 && move_vector.second == 0){ // movement down
            // check that pawn is black
            if (piece.at(0) != 'B')
                return false;

            // check that end piece is empty
            if (end_piece != "  ")
                return false;
        } else if (move_vector.first == -1 && move_vector.second == 0){ // movement up
            // check that pawn is white
            if (piece.at(0) != 'W')
                return false;

            // check that end piece is empty
            if (end_piece != "  ")
                return false;
        } else {
            // all possible pawn moves are enumerated above, so return false;
            return false;
        }

        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        return true;
    }

    return false;
};
