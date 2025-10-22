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
// for the color, a character for the piece type, and a number character for the specific rook it is (needed for castle-ing),
// (i.e. BK = black king,  WR1 = white rook one). 

// There will also be a dead list for white, and a dead list for black, keeping track of what pieces have been removed from the board.
// These will be displayed above and below the rendered board

// There will be six flags: WK_moved, BK_moved, WR1_moved, WR2_moved, BR1_moved, BR2_moved. These let us know if certain rooks or kings
// have moved already for castle-ing.

// A king is the peice that initiates a castle. Simply move the king two spaces to the right or left of where it started to perform a castle.

//////////////////////////////////////
// A step by step rundown of a turn:
//////////////////////////////////////

//  1) When a client takes a turn, they will type the starting position coordinate and the ending position coordinate
//      (i.e. "a2", "d5") which get sent to the server. Then the rest of the steps are in a function called "make_move" which will check that the
//      requested move is valid. If it's valid, the move will be made and the function returns Valid. If a pawn needs to be promoted, it 
//      returns ValidWithReplace. Otherwise it returns Invalid, and the server will prompt the user to make different move.
//  2) These coordinates are then converted to integer coordiantes (i.e. "a2" -> 12, "d5" -> 45).Then the game will convert these integers 
//      into pair<int,int> where the row and column from the user input are swapped as (col,row)->(row,col) to match the layout of the 2d board vector. 
//  3) The function will perform some sanity checks: is the piece being moved the same color as the current player, is there movement at all, and is
//      there not a friendly piece at the endcoordinate
//  4) The difference vector (as a pair<int,int>) will be calculated (i.e. for a bishop moving from (1,3) to (3,5), vector is (3-1,5-3) = (2,2)),
//      or (i.e. for a queen moving left from (3,8) to (3,2), vector is (3-3,2-8) = (0,-6)).
//  5) The type of the piece being moved is compared to the 6 different chess piece types. Inside each of these conditionals, the following happens:
//      5a) The difference vector is examined to see if it's valid for the piece type (and valid for the current position of the piece(s) in the 
//          case of pawns first movements and kings/rooks for castling)
//      5b) if not moving a knight, the difference unit vector is added iteratively to the starting coordiante to check that there aren't any pieces 
//          in the way of the movement.
//      5c) The final destination is examined to see if an opponent's piece is there. If so, it gets added to a deadlist which are rendered above
//          and below the board to imitate the removed pieces lying on the table in a real chess game. Pawns have special cases here, where for 
//          diagonal movement, there must be an opponents piece at the destination.
//      5d) Replace starting tile with empty spaces in board structure 
//      5e) Replace destination tile with the piece we are moving
//      5f) For rooks and kings, set the appropriate flags for their first movements to prevent subsequent castleing
//      5g) Check if a King was removed. If so, set the appriate black_won or white_won flags.
//      5h) For pawns, if reaching the other end of the board, return ValidWithReplace, which tells server to prompt the client for a promotion piece.

Game::Game() : WR1_moved(false), WR2_moved(false), WK_moved(false), BR1_moved(false), BR2_moved(false), BK_moved(false),
    white_won(false), black_won(false), replace_coordinates({0,0}), white_dead_list_idx(0), black_dead_list_idx(0){
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

    white_dead_list.assign(16,"   ");
    black_dead_list.assign(16,"   ");
};

bool Game::get_black_won(){
    return black_won;
}

bool Game::get_white_won(){
    return white_won;
}

// Takes the table structure and the dead lists and compiles them into a pretty chess table inside the buffer passed in
void Game::format_table_to_print(char buf[DEFAULT_BUFLEN]){
    std::vector<std::vector<char>> pretty_table = {
        {' ',' ',black_dead_list[10].at(0),black_dead_list[10].at(1),' ',' ',black_dead_list[11].at(0),black_dead_list[11].at(1),' ',' ',black_dead_list[12].at(0),black_dead_list[12].at(1),' ',' ',black_dead_list[13].at(0),black_dead_list[13].at(1),' ',' ',black_dead_list[14].at(0),black_dead_list[14].at(1),' ',' ',black_dead_list[15].at(0),black_dead_list[15].at(1),' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','\n'},
        {' ',' ',black_dead_list[0].at(0),black_dead_list[0].at(1),' ',' ',black_dead_list[1].at(0),black_dead_list[1].at(1),' ',' ',black_dead_list[2].at(0),black_dead_list[2].at(1),' ',' ',black_dead_list[3].at(0),black_dead_list[3].at(1),' ',' ',black_dead_list[4].at(0),black_dead_list[4].at(1),' ',' ',black_dead_list[5].at(0),black_dead_list[5].at(1),' ',' ',black_dead_list[6].at(0),black_dead_list[6].at(1),' ',' ',black_dead_list[7].at(0),black_dead_list[7].at(1),' ',' ',black_dead_list[8].at(0),black_dead_list[8].at(1),' ',' ',black_dead_list[9].at(0),black_dead_list[9].at(1),' ',' ',' ','\n'},
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
        {' ',' ',' ',' ','a',' ',' ',' ',' ','b',' ',' ',' ',' ','c',' ',' ',' ',' ','d',' ',' ',' ',' ','e',' ',' ',' ',' ','f',' ',' ',' ',' ','g',' ',' ',' ',' ','h',' ',' ',' ','\n'},
        {' ',' ',white_dead_list[0].at(0),white_dead_list[0].at(1),' ',' ',white_dead_list[1].at(0),white_dead_list[1].at(1),' ',' ',white_dead_list[2].at(0),white_dead_list[2].at(1),' ',' ',white_dead_list[3].at(0),white_dead_list[3].at(1),' ',' ',white_dead_list[4].at(0),white_dead_list[4].at(1),' ',' ',white_dead_list[5].at(0),white_dead_list[5].at(1),' ',' ',white_dead_list[6].at(0),white_dead_list[6].at(1),' ',' ',white_dead_list[7].at(0),white_dead_list[7].at(1),' ',' ',white_dead_list[8].at(0),white_dead_list[8].at(1),' ',' ',white_dead_list[9].at(0),white_dead_list[9].at(1),' ',' ',' ','\n'},
        {' ',' ',white_dead_list[10].at(0),white_dead_list[10].at(1),' ',' ',white_dead_list[11].at(0),white_dead_list[11].at(1),' ',' ',white_dead_list[12].at(0),white_dead_list[12].at(1),' ',' ',white_dead_list[13].at(0),white_dead_list[13].at(1),' ',' ',white_dead_list[14].at(0),white_dead_list[14].at(1),' ',' ',white_dead_list[15].at(0),white_dead_list[15].at(1),' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','\n'}
    };

    for (int i = 0; i < PRINTED_BOARD_ROWS; i++){
        for (int j = 0; j < PRINTED_BOARD_COLS; j++){
            buf[i*PRINTED_BOARD_COLS+j] = pretty_table[i][j];
        }
    }
}

// quick helper function to validate user input for a pawn promotion
bool Game::validate_promotion_input(char buf[DEFAULT_BUFLEN]){
    if ((buf[0]=='R' || buf[0]=='N' || buf[0]=='B' || buf[0]=='Q') && buf[1]=='\n' && buf[2]=='\0')
        return true;
    else 
        return false;
}

// Promotes pawn when it reaches the other side of the board
void Game::promote_pawn(char new_piece){
    std::string replacement = "  ";
    if (replace_coordinates.first == 0) // if promoting a white pawn
        replacement[0] = 'W';
    else 
        replacement[0] = 'B';
    replacement[1] = new_piece;
    table[replace_coordinates.first][replace_coordinates.second] = replacement;
}

// makes a move and returns whether it was invalid, valid, or if a pawn was moved to the other side and needs to be promoted
MoveResult Game::make_move(char buf[DEFAULT_BUFLEN], char player_color){
    bool attempting_left_castle; // this will refer to castles on the left side of the board, i.e. BK and BR1, or WK and WR1
    bool attempting_right_castle; // this will refer to castles on the right side of the board, i.e. BK and BR2, or WK and WR2
    std::string piece; // the piece being moved
    std::string end_piece; // the piece (or blank space) at the end coordinate

    //////////////////////////////////////
    //// Checking input /////
    //////////////////////////////////////
    if (buf[0] < 'a' || buf[0] > 'h') return MoveResult::Invalid;
    if (buf[1] < '1' || buf[1] > '8') return MoveResult::Invalid;
    if (buf[2] < 'a' || buf[2] > 'h') return MoveResult::Invalid;
    if (buf[3] < '1' || buf[3] > '8') return MoveResult::Invalid;
    if (buf[4] != '\n' && buf[5] != '\0') return MoveResult::Invalid;

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
        return MoveResult::Invalid;


    //////////////////////////////////////
    //// Creating move vector /////
    //////////////////////////////////////
    std::pair<int,int> move_vector = {end_coord.first-start_coord.first, end_coord.second-start_coord.second};

    // sanity check: if not moving at all, return MoveResult::Invalid
    if (move_vector.first == 0 && move_vector.second == 0) 
        return MoveResult::Invalid;

    // sanity check: if friendly piece at endcoord, return MoveResult::Invalid
    if (piece.at(0) == end_piece.at(0))
        return MoveResult::Invalid;
    
    //////////////////////////////////////
    //// Handling rook movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'R'){
        if (move_vector.first != 0 && move_vector.second != 0) // either row-movement or col-movement must be 0 for a rook
            return MoveResult::Invalid;

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
                return MoveResult::Invalid;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W'){
            white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
            white_dead_list_idx++;
        } else if (end_piece.at(0) == 'B'){
            black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
            black_dead_list_idx++;
        }

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

        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        return MoveResult::Valid;
    } 

    //////////////////////////////////////
    //// Handling knight movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'N'){
        bool valid_flag = false;
        for (std::pair<int,int> p : knight_move_vectors){ // loop through all the possible knight movement vectors
            if (p == move_vector){ // if we get a match with the requested move vector
                valid_flag = true;
                break;
            }
        }
        if (!valid_flag)
            return MoveResult::Invalid;

        // check if we're removing a piece
        if (end_piece.at(0) == 'W'){
            white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
            white_dead_list_idx++;
        } else if (end_piece.at(0) == 'B'){
            black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
            black_dead_list_idx++;
        }

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;
        
        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        return MoveResult::Valid;
    } 

    //////////////////////////////////////
    //// Handling bishop movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'B'){
        if (std::abs(move_vector.first) != std::abs(move_vector.second)) // for diagonal movement, row-movement and col-movement must be equal
            return MoveResult::Invalid;
        
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
                return MoveResult::Invalid;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W'){
            white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
            white_dead_list_idx++;
        } else if (end_piece.at(0) == 'B'){
            black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
            black_dead_list_idx++;
        }

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        return MoveResult::Valid;
    } 
    
    //////////////////////////////////////
    //// Handling queen movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'Q'){
        if (move_vector.first != 0 && move_vector.second != 0){ // if not straight line movement
            if (std::abs(move_vector.first) != std::abs(move_vector.second)) // if not diagonal movement
                return MoveResult::Invalid;
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
                return MoveResult::Invalid;
        } while (iterative_coord != end_coord);

        // check if we're removing a piece
        if (end_piece.at(0) == 'W'){
            white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
            white_dead_list_idx++;
        } else if (end_piece.at(0) == 'B'){
            black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
            black_dead_list_idx++;
        }

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        return MoveResult::Valid;
    } 
    
    //////////////////////////////////////
    //// Handling king movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'K'){
        if (move_vector.second==-2 && move_vector.first==0){ // first we check for left castleing
            if (piece.at(0)=='W' && !WK_moved  && !WR1_moved){ // attempting white left castle
                // check for collisions between the white king and the left rook
                if (table[7][3] != "  " || table[7][2] != "  " || table[7][1] != "  ")
                    return MoveResult::Invalid;
                // make move
                table[7][2] = "WK";
                table[7][3] = "WR1";
                table[7][4] = "  ";
                table[7][0] = "  ";
                WK_moved = true;
                return MoveResult::Valid;
            } else if (piece.at(0)=='B' && !BK_moved  && !BR1_moved){ // attempting black left castle
                // check for collisions between the black king and the left rook
                if (table[0][3] != "  " || table[0][2] != "  " || table[0][1] != "  ")
                    return MoveResult::Invalid;
                // make move
                table[0][2] = "BK";
                table[0][3] = "BR1";
                table[0][4] = "  ";
                table[0][0] = "  ";
                BK_moved = true;
                return MoveResult::Valid;
            }
        } else if (move_vector.second==2 && move_vector.first==0){ // then we check for right castleing
            if (piece.at(0)=='W' && !WK_moved  && !WR2_moved){ // attempting white right castle
                // check for collisions between the white king and the right rook
                if (table[7][5] != "  " || table[7][6] != "  ")
                    return MoveResult::Invalid;
                // make move
                table[7][6] = "WK";
                table[7][5] = "WR2";
                table[7][4] = "  ";
                table[7][7] = "  ";
                WK_moved = true;
                return MoveResult::Valid;
            } else if (piece.at(0)=='B' && !BK_moved  && !BR2_moved){ // attempting black right castle
                // check for collisions between the black king and the right rook
                if (table[0][5] != "  " || table[0][6] != "  ")
                    return MoveResult::Invalid;
                // make move
                table[0][5] = "BK";
                table[0][6] = "BR2";
                table[0][4] = "  ";
                table[0][7] = "  ";
                BK_moved = true;
                return MoveResult::Valid;
            }
        } else if (std::abs(move_vector.first) > 1 || std::abs(move_vector.second) > 1){ // If not castleing, we check
// that king is only moving 1 tile away
            return MoveResult::Invalid;
        }

        // if removing a piece, add it to dead list
        if (end_piece.at(0) == 'W'){
            white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
            white_dead_list_idx++;
        } else if (end_piece.at(0) == 'B'){
            black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
            black_dead_list_idx++;
        }
        

        // set king moved flag
        if (piece.at(0) == 'W')
            WK_moved = true;
        else if (piece.at(0) == 'B')
            BK_moved = true;


        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        // make move
        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        return MoveResult::Valid;
    } 
    
    //////////////////////////////////////
    //// Handling pawn movement /////
    //////////////////////////////////////
    if (piece.at(1) == 'P'){
        // if player wants to move pawn two spaces
        if (move_vector.first == 2 && move_vector.second == 0){ // 2 is movement towards white side
            // check that pawn is of color and position to perform such a move
            if (!(piece.at(0) == 'B' && start_coord.first == 1))
                return MoveResult::Invalid;

            // check if there's a collision
            if (table[start_coord.first+1][start_coord.second] != "  ")
                return MoveResult::Invalid;
        } else if (move_vector.first == -2  && move_vector.second == 0){ // -2 is movement towards black side
            // check that pawn is of color and position to perform such a move
            if (!(piece.at(0) == 'W' && start_coord.first == 6))
                return MoveResult::Invalid;

            // check if there's a collision
            if (table[start_coord.first-1][start_coord.second] != "  ")
                return MoveResult::Invalid;
        } else if (move_vector.first == 1 && move_vector.second == 1){ // movement down and right
            // check that pawn is black
            if (piece.at(0) != 'B')
                return MoveResult::Invalid;

            // piece at end_coordinate must be white
            if (end_piece.at(0) == 'W'){
                white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
                white_dead_list_idx++;
            } else 
                return MoveResult::Invalid;

        } else if (move_vector.first == 1 && move_vector.second == -1){ // movement down and left
            // check that pawn is black
            if (piece.at(0) != 'B')
                return MoveResult::Invalid;

            // piece at end_coordinate must be white
            if (end_piece.at(0) == 'W'){
                white_dead_list.insert(white_dead_list.begin()+white_dead_list_idx,end_piece);
                white_dead_list_idx++;
            } else 
                return MoveResult::Invalid;

        } else if (move_vector.first == -1 && move_vector.second == 1){ // movement up and right
            // check that pawn is white
            if (piece.at(0) != 'W')
                return MoveResult::Invalid;

            // piece at end_coordinate must be black
            if (end_piece.at(0) == 'B'){
                black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
                black_dead_list_idx++;
            } else 
                return MoveResult::Invalid;

        } else if (move_vector.first == -1 && move_vector.second == -1){ // movement up and left
            // check that pawn is white
            if (piece.at(0) != 'W')
                return MoveResult::Invalid;

            // piece at end_coordinate must be black
            if (end_piece.at(0) == 'B'){
                black_dead_list.insert(black_dead_list.begin()+black_dead_list_idx,end_piece);
                black_dead_list_idx++;
            } else 
                return MoveResult::Invalid;
        } else if (move_vector.first == 1 && move_vector.second == 0){ // movement down
            // check that pawn is black
            if (piece.at(0) != 'B')
                return MoveResult::Invalid;

            // check that end piece is empty
            if (end_piece != "  ")
                return MoveResult::Invalid;
        } else if (move_vector.first == -1 && move_vector.second == 0){ // movement up
            // check that pawn is white
            if (piece.at(0) != 'W')
                return MoveResult::Invalid;

            // check that end piece is empty
            if (end_piece != "  ")
                return MoveResult::Invalid;
        } else {
            // all possible pawn moves are enumerated above, so return MoveResult::Invalid;
            return MoveResult::Invalid;
        }

        table[start_coord.first][start_coord.second] = "  ";
        table[end_coord.first][end_coord.second] = piece;

        // check if game ends
        if (end_piece == "WK")
            black_won = true;
        else if (end_piece == "BK")
            white_won = true;

        if (end_coord.first == 7 || end_coord.first == 0){ // if at the top or bottom of board, player will get to replace pawn
            replace_coordinates = end_coord;
            return MoveResult::ValidWithReplace;
        }


        return MoveResult::Valid;
    }

    return MoveResult::Invalid;
};
