#include <vector>
#include <string>
#include <map>
#include <utility>
#include "utils.h"


#define PRINTED_BOARD_ROWS 22
#define PRINTED_BOARD_COLS 44
#define PRINTED_BOARD_SIZE PRINTED_BOARD_ROWS * PRINTED_BOARD_COLS



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

class Game {
    public:
        Game();
        bool get_white_won();
        bool get_black_won();

        // makes a move and returns whether it was invalid, valid, or if a pawn was moved to the other side and needs to be promoted
        MoveResult make_move(char buf[DEFAULT_BUFLEN], char player_color); 

        // Takes the table structure and the dead lists and compiles them into a pretty chess table inside the buffer passed in
        void format_table_to_print(char buf[DEFAULT_BUFLEN]);

        // Promotes pawn when it reaches the other side of the board
        void promote_pawn(char new_piece);

        // quick helper function to validate user input for a pawn promotion
        bool static validate_promotion_input(char buf[DEFAULT_BUFLEN]);
    private:
        bool WR1_moved, WR2_moved, WK_moved; // White rooks and white king-moved flags
        bool BR1_moved, BR2_moved, BK_moved; // black rooks and black king-moved flags

        bool white_won, black_won; // flags for if white won or if black one

        std::pair<int,int> replace_coordinates; // coordinates of the pawn to be promoted

        std::vector<std::vector<std::string>> table; // The 8x8 table used by the code

        std::vector<std::pair<int,int>> knight_move_vectors; // a map of all the valid movement vectors for a knight

        std::vector<std::string> white_dead_list, black_dead_list;
        int white_dead_list_idx, black_dead_list_idx; // Dead lists will be initialized to a fixed size of 16, so we need to keep track of the
        // insertion index since we cant just push_back().
};