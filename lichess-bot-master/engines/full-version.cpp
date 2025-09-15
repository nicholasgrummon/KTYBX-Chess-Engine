/////////////////////////////////////////////////////////////////////////////////////
// KTY-BX COMMAND LINE CHESS ENGINE
/////////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <fstream> // TODO: remove?
#include <bitset>
#include <ctime>
#include <chrono>
int timer1 = 0;
int timer2 = 0;
int timer3 = 0;
int called = 0;

/////////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES - ENGINE SETTINGS AND INCENTIVES
// >>> tune engine here <<<
/////////////////////////////////////////////////////////////////////////////////////
// settings
const int MAX_DEPTH = 5;              // set an evaluation depth
const int MAX_SEARCH_TIME = 5;         // limit the engine's search time (in seconds)
const int MAX_TREE_WIDTH = 150;        // provide a max branching factor (must be >= 35)
const int Q_EXPANSION_FACTOR = 3;      // expand quiescence search up to 3 times deeper
const int STABILITY_WINDOW = 30;       // q-search must add at least this much value
const int HASH_TABLE_LENGTH = 1048583; // select prime number close to 1M to reduce hash collisions
const int ENDGAME_CUTOFF = 4;          // use endgame settings when there are less pieces

int iteration_depth = MAX_DEPTH;

// piece values
int P_VAL = 100; 
int N_VAL = 300; 
int B_VAL = 330; 
int R_VAL = 490; 
int Q_VAL = 900; 
int K_VAL = 10000;
int VAL[6] = { P_VAL,  N_VAL,  B_VAL,  R_VAL,  Q_VAL,  K_VAL };

// evaluation bonuses
int CASTLE_BONUS = 100;
int DRAW_PENALTY = 100;
const int PST_WEIGHT = 5;
const int MOBILITY_INCENTIVE = 5;

// move ordering bonuses
int MIDDLE_BONUS = 12;
int AUXMID_BONUS = 8;
int EDGE_PENALTY = 8;

// define players
int player; int computer;
bool w_resignation; bool is_castled_w;
bool b_resignation; bool is_castled_b;

// summary statistics
int m_nodes = 0; int q_nodes = 0; int total_nodes = 0;
int cut_offs = 0; int late_move_reductions = 0;
int hash_entries = 0;
int max_quiescence_search_depth = 0;

/////////////////////////////////////////////////////////////////////////////////////
// ENGINE CONTAINERS AND STORAGE IDs
// the engine uses these tables to keep track of the board and move search. The 
// associated variables are simply to make indexing more intuitive
/////////////////////////////////////////////////////////////////////////////////////
// continuations
int game_continuation[120];
int principal_variation[MAX_DEPTH]; // stored by dist away from root so move can be used in any iteration
int current_variation[MAX_DEPTH];

// move generation
int moves_list[2*MAX_DEPTH][MAX_TREE_WIDTH];
int values_list[2*MAX_DEPTH][MAX_TREE_WIDTH];
int capture_sequence[2*MAX_DEPTH + 120];
// MAX_DEPTH spaces alloted for minimax
// MAX_DEPTH spaces alloted for quiescence
// 120 spaces alloted for surface game

// move ordering
int layer_best_moves[MAX_DEPTH];
int layer_previous_evals[MAX_DEPTH][4096]; // 64*64 possible move vectors
int layer_killer_moves[MAX_DEPTH];

/////////////////////////////////////////////////////////////////////////////////////
// CONSTANT BITBOARDS
// define the chess board
/////////////////////////////////////////////////////////////////////////////////////
uint64_t RANK_1 = 0b0000000000000000000000000000000000000000000000000000000011111111;
uint64_t RANK_2 = 0b0000000000000000000000000000000000000000000000001111111100000000;
uint64_t RANK_3 = 0b0000000000000000000000000000000000000000111111110000000000000000;
uint64_t RANK_4 = 0b0000000000000000000000000000000011111111000000000000000000000000;
uint64_t RANK_5 = 0b0000000000000000000000001111111100000000000000000000000000000000;
uint64_t RANK_6 = 0b0000000000000000111111110000000000000000000000000000000000000000;
uint64_t RANK_7 = 0b0000000011111111000000000000000000000000000000000000000000000000;
uint64_t RANK_8 = 0b1111111100000000000000000000000000000000000000000000000000000000;

uint64_t FILE_A = 0b1000000010000000100000001000000010000000100000001000000010000000;
uint64_t FILE_B = 0b0100000001000000010000000100000001000000010000000100000001000000;
uint64_t FILE_C = 0b0010000000100000001000000010000000100000001000000010000000100000;
uint64_t FILE_D = 0b0001000000010000000100000001000000010000000100000001000000010000;
uint64_t FILE_E = 0b0000100000001000000010000000100000001000000010000000100000001000;
uint64_t FILE_F = 0b0000010000000100000001000000010000000100000001000000010000000100;
uint64_t FILE_G = 0b0000001000000010000000100000001000000010000000100000001000000010;
uint64_t FILE_H = 0b0000000100000001000000010000000100000001000000010000000100000001;

uint64_t MIDDLE = 0b0000000000000000000000000011100000111000000000000000000000000000;
uint64_t AUXMID = 0b0000000000000000001111000010010000100100001111000000000000000000;
uint64_t EDGE   = 0b0000000000000000100000011000000110000001100000010000000000000000;

uint64_t W_SHORT_CASTLE_ZONE = (3ULL << 1);
uint64_t W_LONG_CASTLE_ZONE = (7ULL << 4);

uint64_t B_SHORT_CASTLE_ZONE = (3ULL << 57);
uint64_t B_LONG_CASTLE_ZONE = (7ULL << 60);

/////////////////////////////////////////////////////////////////////////////////////
// PIECE BITBOARDS
/////////////////////////////////////////////////////////////////////////////////////
uint64_t pos[15]; // piece position look-up table
// piece IDs
int wP=0; int wN=1; int wB=2; int wR=3; int wQ=4; int wK=5;
int bP=6; int bN=7; int bB=8; int bR=9; int bQ=10; int bK=11;
int empty=12; int white=13; int black=14;

// castling and promotion IDs
int w_castle_short = 13; int w_castle_long = 14;
int b_castle_short = 15; int b_castle_long = 16;
int en_passant_key = 17; int promotion_key = 18;

uint64_t en_passant_w; uint64_t en_passant_b;
uint64_t checks[2] = {0, 0};

void clear_board() {
    for (int piece_ID=0; piece_ID<15; piece_ID++) {
        pos[piece_ID] = 0;
    }
}

void new_game() {
    en_passant_w = 0; en_passant_b = 0;
    bool w_resignation = false; bool is_castled_w = false;
    bool b_resignation = false; bool is_castled_b = false;
    pos[wP] = 0b0000000000000000000000000000000000000000000000001111111100000000;
    pos[wN] = 0b0000000000000000000000000000000000000000000000000000000001000010;
    pos[wB] = 0b0000000000000000000000000000000000000000000000000000000000100100;
    pos[wR] = 0b0000000000000000000000000000000000000000000000000000000010000001;
    pos[wQ] = 0b0000000000000000000000000000000000000000000000000000000000010000;
    pos[wK] = 0b0000000000000000000000000000000000000000000000000000000000001000;
        
    pos[bP] = 0b0000000011111111000000000000000000000000000000000000000000000000;
    pos[bN] = 0b0100001000000000000000000000000000000000000000000000000000000000;
    pos[bB] = 0b0010010000000000000000000000000000000000000000000000000000000000;
    pos[bR] = 0b1000000100000000000000000000000000000000000000000000000000000000;
    pos[bQ] = 0b0001000000000000000000000000000000000000000000000000000000000000;
    pos[bK] = 0b0000100000000000000000000000000000000000000000000000000000000000;
        
    pos[empty] = 0b0000000000000000111111111111111111111111111111110000000000000000;
    pos[white] = 0b0000000000000000000000000000000000000000000000001111111111111111;
    pos[black] = 0b1111111111111111000000000000000000000000000000000000000000000000;
}

void update_colors() {
    pos[white] = (pos[wP] | pos[wN] | pos[wB] | pos[wR] | pos[wQ] | pos[wK]);
    pos[black] = (pos[bP] | pos[bN] | pos[bB] | pos[bR] | pos[bQ] | pos[bK]);
    pos[empty] = ~(pos[white] | pos[black]);
}

void read_FEN(std::string fen) {
    /* FEN is the primary input method for UCI communication */

    // clear board
    clear_board();

    // read 'piece placement' field
    int chr = 0, sqr = 0;
    while (fen[chr] != ' ') {
        // if white piece
        if (fen[chr] == 'P') { pos[wP] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'N') { pos[wN] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'B') { pos[wB] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'R') { pos[wR] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'Q') { pos[wQ] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'K') { pos[wK] |= (1ULL << (63-sqr)); }

        // if black piece
        else if (fen[chr] == 'p') { pos[bP] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'b') { pos[bB] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'r') { pos[bR] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'q') { pos[bQ] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'k') { pos[bK] |= (1ULL << (63-sqr)); }
        else if (fen[chr] == 'n') { pos[bN] |= (1ULL << (63-sqr)); }

        // if line break
        else if (fen[chr] == '/') { sqr--; }
        
        // if number, assign empty squares
        else {
            for (int i=0; i<(fen[chr] - '0'); i++) {
                pos[empty] |= (1ULL << (63-sqr));
                sqr++;
            }
            sqr--;
        }
        chr++;
        sqr++;
    }
    update_colors();

    // read 'active color' field
    chr += 1;
    if (fen[chr] == 'w') { computer = white; player = black; }
    else { computer = black; player = white; }
    
    // read 'castling availability' field
    chr += 2;
    is_castled_w = true;
    is_castled_b = true;
    while (fen[chr] != ' ') {
        if (fen[chr] == 'K' || fen[chr] == 'Q') { is_castled_w = false; }
        else if (fen[chr] == 'k' || fen[chr] == 'q') { is_castled_b = false; }
        chr++;
    }

    // skip 'en passant target square' field
    // TODO: add this functionality
    chr += 1;
    while (fen[chr] != ' ') { chr++; }

    // skip 'halfmove clock' field
    // TODO: add this functionality (draw if halfmove >= 50)
    chr += 1;
    while (fen[chr] != ' ') { chr++; }

    // skip 'fullmove clock' field, no need for move number
}

/////////////////////////////////////////////////////////////////////////////////////
// BITBOARD AND MISCELLANEOUS OPERATIONS
/////////////////////////////////////////////////////////////////////////////////////
void print_board() {
    int rank = 8;
    std::cout << "    KITTY BOX CHESS" << std::endl;
    std::cout << "   -----------------" << std::endl;
    for (int i=63; i>=0; i--) {
        if (i%8 == 7) { std::cout << rank << " | "; rank--; } // print edge
        if (pos[wP] & (1ULL << i)) { std::cout << "o "; } // print white pieces
        else if (pos[wN] & (1ULL << i)) { std::cout << "N "; }
        else if (pos[wB] & (1ULL << i)) { std::cout << "B "; }
        else if (pos[wR] & (1ULL << i)) { std::cout << "R "; }
        else if (pos[wQ] & (1ULL << i)) { std::cout << "Q "; }
        else if (pos[wK] & (1ULL << i)) { std::cout << "K "; }

        else if (pos[bP] & (1ULL << i)) { std::cout << "x "; } // print black pieces
        else if (pos[bN] & (1ULL << i)) { std::cout << "n "; }
        else if (pos[bB] & (1ULL << i)) { std::cout << "b "; }
        else if (pos[bR] & (1ULL << i)) { std::cout << "r "; }
        else if (pos[bQ] & (1ULL << i)) { std::cout << "q "; }
        else if (pos[bK] & (1ULL << i)) { std::cout << "k "; }

        else if (pos[empty] & (1ULL << i)) { std::cout << ". "; }

        if (i%8 == 0) { std::cout << "| " << std::endl; }

    }
    std::cout << "   -----------------" << std::endl;
    std::cout << "    A B C D E F G H  " << std::endl;
}

int bit_scan_left(uint64_t bits) {
    // return position of first non-zero bit scanning right to left
    int i = 0;
    while ((((1ULL << i) & bits) == 0) && (i < 64)) { i++; }
    return i;
}

int bit_scan_right(uint64_t bits) {
    // return position of first non-zero bit scanning left to right
    int i = 63;
    while ((((1ULL << i) & bits) == 0) && (i>=0)) { i--; }
    return i;
}

int count(uint64_t bits) {
    // count number of non-zero bits
    int count = 0;
    while (bits) {
        count += (bits & 1ULL);
        bits = bits >> 1;
    }
    return count;
}

int opp(int color) {
    // change sides
    if (color == white) { return black; }
    else { return white; }
}

uint64_t uint64_t_rand() {
    // generate random 64 bit number, used to initialize hashes
    uint64_t num = 0;
    for (int i=0; i<64; i++) { 
        num = (num*2 + rand()%2);
    }
    return num;
}

void print_coords(int move) {
    int origination = (move >> 6);
    int destination = (move & 63);
    std::cout << char('h' - origination%8);
    std::cout << char('1' + origination/8);
    std::cout << char('h' - destination%8);
    std::cout << char('1' + destination/8);
}

int material_value_at(int index) {
    for (int i=0; i<12; i++) {
        if (pos[i] & (1ULL << index)) {
            return VAL[i%6];
        }
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
// PIECE SQUARE TABLES
// give each square a positional weight depending on the piece.
/////////////////////////////////////////////////////////////////////////////////////
int PST[7][64] = {
    // pawn table
    {  0,  0,  0,  0,  0,  0,  0,  0,
      10, 10, 10, 10, 10, 10, 10, 10,
       2,  2,  4,  6,  6,  4,  2,  2,
       1,  1,  2,  5,  5,  2,  1,  1,
       0,  0,  4,  4,  5,  0,  0,  0,
       1, -1, -2,  0,  0, -2, -1,  1,
       1,  2,  2, -5, -5,  2,  2,  1,
       0,  0,  0,  0,  0,  0,  0,  0 },
    // knight table
    {-10, -8, -6, -6, -6, -6, -8,-10,
      -8, -4,  0,  0,  0,  0, -4, -8,
      -6,  0,  2,  3,  3,  2,  0, -6,
      -6,  1,  3,  4,  4,  3,  1, -6,
      -6,  0,  3,  4,  4,  3,  0, -6,
      -6,  1,  2,  3,  3,  3,  1, -6,
      -8, -4,  0,  1,  1,  0, -4, -8,
     -10, -6, -4, -6, -6, -4, -6,-10 },
    // bishop table
    {-4, -2, -2, -2, -2, -2, -2, -4,
     -2,  0,  0,  0,  0,  0,  0, -2,
     -2,  0,  1,  2,  2,  1,  0, -2,
     -2,  1,  1,  2,  2,  1,  1, -2,
     -2,  0,  2,  2,  2,  2,  0, -2,
     -2,  2,  2,  2,  2,  2,  2, -2,
     -2,  5,  0,  0,  0,  0,  5, -2,
     -4, -2, -8, -2, -2, -8, -2, -4 },
    // rook table
    { 0,  0,  0,  0,  0,  0,  0,  0,
      1,  2,  2,  2,  2,  2,  2,  1,
     -1,  0,  0,  0,  0,  0,  0, -1,
     -1,  0,  0,  0,  0,  0,  0, -1,
     -1,  0,  0,  0,  0,  0,  0, -1,
     -1,  0,  0,  0,  0,  0,  0, -1,
     -1,  0,  0,  0,  0,  0,  0, -1,
      0,  0,  0,  1,  1,  0,  0,  0 },
    //queen table
    {-4, -2, -2, -1, -1, -2, -2, -4,
     -2,  0,  0,  0,  0,  0,  0, -2,
     -2,  0,  1,  1,  1,  1,  0, -2,
     -1,  0,  1, -1, -1,  1,  0, -1,
      0,  0,  1,  0,  0,  1,  0, -1,
     -2,  1,  1,  1,  1,  1,  0, -2,
     -2,  0,  1,  0,  0,  0,  0, -2,
     -4, -2, -2,  5, -1, -2, -2, -4 },
    //king table
    {-6, -8, -8, -8,-10, -8, -8, -6,
     -6, -8, -8,-10,-10, -8, -8, -6,
     -6, -8, -8,-10,-10, -8, -8, -6,
     -6, -8, -8,-10,-10, -8, -8, -6,
     -4, -6, -6, -8, -8, -6, -6, -4,
     -2, -4, -4, -4, -4, -4, -4, -2, 
      4,  4,  0,  0,  0,  0,  4,  4,
      4,  5,  6,  0,  0,  2,  8,  4 },
    // king endgame table
    {-10, -8, -6, -4, -4, -6, -8,-10,
      -6, -4, -2,  0,  0, -2, -4, -6,
      -6, -2,  4,  6,  6,  4, -2, -6,
      -6, -2,  6,  8,  8,  6, -2, -6,
      -6, -2,  6,  8,  8,  6, -2, -6,
      -6, -2,  4,  6,  6,  4, -2, -6,
      -6, -6,  0,  0,  0,  0, -6, -6,
     -10, -6, -6, -6, -6, -6, -6,-10 }
};

int black_pst_index(int white_index) {
    // reflection across the center line
    return ((white_index + 56) - ((white_index / 8) * 16));
}

void scale_PST() {
    for (int i=0; i<7; i++) {
        for (int j=0; j<64; j++) {
            PST[i][j] = PST_WEIGHT * PST[i][j];
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// RAYS
// this array stores the "rays" pointing away from every square to help with
// generating moves for sliding pieces
/////////////////////////////////////////////////////////////////////////////////////
uint64_t RAYS[64][8]; // sliding moves array
int nrt=0; int sth=1; int est=2; int wst=3;
int nrtest=4; int nrtwst=5; int sthest=6; int sthwst=7;

void add_ray(int index, uint64_t boundary, int vector, int mem_index) {
    // constant bitboards are used to bound rays within the board
    uint64_t square = (1ULL << index); uint64_t ray = 0;

    if ((square & boundary) == 0) {
        for (int n=1; n<8; n++) {
            if (vector > 0) { ray |= (square << vector*n); }
            if (vector < 0) { ray |= (square >> (-vector)*n); }

            if (ray & boundary) { break; }
        }
    }
    RAYS[index][mem_index] = ray;
}

void fill_RAYS() {
    for (int i=0; i<64; i++) {
        // cardinal rays
        add_ray(i, RANK_8,  8, nrt); // north
        add_ray(i, RANK_1, -8, sth); // south
        add_ray(i, FILE_H, -1, est); // east
        add_ray(i, FILE_A,  1, wst); // west

        // boundary is composite for secondary rays
        add_ray(i, (FILE_H | RANK_8),  7, nrtest); // north east
        add_ray(i, (FILE_A | RANK_8),  9, nrtwst); // north west
        add_ray(i, (FILE_H | RANK_1), -9, sthest); // south east
        add_ray(i, (FILE_A | RANK_1), -7, sthwst); // south west
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// TRANSPOSITION TABLE AND ZOBRIST KEYS
// using a 64 bit random number assigned to every piece and every square, a 
// psuedo-unique zobrist key can be generated for any position.  This key can then be
// used to store information about positions in a hash table
/////////////////////////////////////////////////////////////////////////////////////
// TODO: hash table getting collisions at depth 4+
uint64_t HASH_TABLE[HASH_TABLE_LENGTH][4];
int p_key=0; int p_eval=1; int p_depth=2; int p_best=3;

// zobrist key initialization tables
uint64_t PIECE_TABLE[12][64];
uint64_t EN_PASSANT_TABLE[64];
uint64_t SIDE;

void seed_tables() {
    SIDE = uint64_t_rand();
    for (int i=0; i<64; i++) {
        EN_PASSANT_TABLE[i] = uint64_t_rand();
        for (int piece=0; piece<12; piece++) {
            PIECE_TABLE[piece][i] = uint64_t_rand();
        }
    }
}

uint64_t gen_zobrist_key(int side_to_move) {
    uint64_t key = 0;
    if (side_to_move == black) {
        key ^= SIDE;
    }

    for (int i=0; i<64; i++) {
        if ((en_passant_w | en_passant_b) & (1ULL << i)) {
            key ^= EN_PASSANT_TABLE[i];
        }

        for (int piece=0; piece<12; piece++) {
            if (pos[piece] & (1ULL << i)) {
                key ^= PIECE_TABLE[piece][i];
            }
        }
    }

    return key;
}

int gen_hash_index(uint64_t key) {
    return (key % HASH_TABLE_LENGTH);
}

void clear_hash_table() {
    for (int i=0; i<HASH_TABLE_LENGTH; i++) {
        for (int j=0; j<3; j++) {
            HASH_TABLE[i][j] = 0;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// MOVE GENERATION
// algorithms for generating legal moves for each piece
/////////////////////////////////////////////////////////////////////////////////////
uint64_t gen_wP_forward(int i) {
    // white pawns move up one or two squares if they are not blocked
    uint64_t bits = (1ULL << i);
    uint64_t moves = ((bits << 8) | ((bits & RANK_2) << 16));
    uint64_t blockers = (moves & (~pos[empty]));
    moves ^= ((blockers | (blockers << 8)) & moves);

    return moves;
}

uint64_t gen_wP_attks(int i) { 
    // white pawns attack diagonally up if there is a piece to take
    uint64_t bits = (1ULL << i);
    if (bits & FILE_A) { return (bits << 7); }
    else if (bits & FILE_H) { return (bits << 9); }
    else { return ((bits << 9) | (bits << 7)); }
}

uint64_t gen_wP_moves(int i) {
    return (gen_wP_forward(i) | (gen_wP_attks(i) & (pos[black] | en_passant_b)));
}

uint64_t gen_bP_forward(int i) {
    // black pawns move down one or two squares if they are not blocked
    uint64_t bits = (1ULL << i);
    uint64_t moves = ((bits >> 8) | ((bits & RANK_7) >> 16));
    uint64_t blockers = (moves & (~pos[empty]));
    moves ^= ((blockers | (blockers >> 8)) & moves);

    return moves;
}

uint64_t gen_bP_attks(int i) { 
    // black pawns attack diagonally down if there is a piece to take
    uint64_t bits = (1ULL << i);
    if (bits & FILE_A) {
        return (bits >> 9);
    }

    else if (bits & FILE_H) {
        return (bits >> 7);
    }

    else {
        return ((bits >> 7) | (bits >> 9));
    }
}

uint64_t gen_bP_moves(int i) {
    return (gen_bP_forward(i) | (gen_bP_attks(i) & (pos[white] | en_passant_w)));
}

uint64_t gen_N_moves(int i) {
    // knights jump with a 2x3 pattern once in any direction if they stay in bounds
    uint64_t bits = (1ULL << i); 

    if (bits & FILE_A) { 
        return ((bits << 15) | (bits << 6) | (bits >> 10) | (bits >> 17));
    }

    else if (bits & FILE_B) {
        return ((bits << 17) | (bits << 15) | (bits << 6) | (bits >> 10) | (bits >> 15) | (bits >> 17));
    }

    else if (bits & FILE_G) {
        return ((bits << 17) | (bits << 15) | (bits << 10) | (bits >> 6) | (bits >> 15) | (bits >> 17));
    }

    else if (bits & FILE_H) {
        return ((bits << 17) | (bits << 10) | (bits >> 6) | (bits >> 15));
    }

    else {
        return ((bits << 17) | (bits << 15) | (bits << 10) | (bits << 6) | (bits >> 6) | (bits >> 10) | (bits >> 15) | (bits >> 17));
    }
}

uint64_t gen_K_moves(int i) {
    // king moves one square in any direction if it stays in bounds or castles if legal
    uint64_t bits = (1ULL << i);
    uint64_t moves = 0;
    int color;
    
    if (pos[white] & (1ULL << i)) { color = white; }
    else { color = black; }

    moves |= ((bits << 9) | (bits << 8) | (bits << 7) | (bits << 1) | (bits >> 1) | (bits >> 7) | (bits >> 8) | (bits >> 9));

    if (bits & FILE_A) { moves ^= ((bits << 9) | (bits << 1) | (bits >> 7)); }
    else if (bits & FILE_H) { moves ^= ((bits << 7) | (bits >> 1) | (bits >> 9)); }

    if ((color == white) && (is_castled_w == false)) {
        // short castle
        if ((i == 3) && (pos[wR] & (1ULL << 0)) && ((pos[empty] & W_SHORT_CASTLE_ZONE) == W_SHORT_CASTLE_ZONE)) {
            moves |= (1ULL << 1);
        }
        // long castle
        if ((i == 3) && (pos[wR] & (1ULL << 7)) && ((pos[empty] & W_LONG_CASTLE_ZONE) == W_LONG_CASTLE_ZONE)) {
            moves |= (1ULL << 5);
        }
    }

    if ((color == black) && (is_castled_b == false)) {
        // short castle
        if ((i == 59) && (pos[bR] & (1ULL << 56)) && ((pos[empty] & B_SHORT_CASTLE_ZONE) == B_SHORT_CASTLE_ZONE)) {
            moves |= (1ULL << 57);
        }
        // long castle
        if ((i == 59) && (pos[bR] & (1ULL << 63)) && ((pos[empty] & B_LONG_CASTLE_ZONE) == B_LONG_CASTLE_ZONE)) {
            moves |= (1ULL << 61);
        }
    }

    return moves;
}

uint64_t gen_R_moves(int i) {
    // rooks move along rank and file rays if they are not blocked
    uint64_t moves = 0;
    int blocker_index;
    // TODO: understand why I had the following lines instead of just blockers = ~pos[empty]
    // uint64_t blockers;
    // if (pos[white] & (1ULL << i)) { blockers = ~(pos[empty] | pos[bK]); }
    // else { blockers = ~(pos[empty] | pos[wK]); }
    uint64_t blockers = ~pos[empty];

    moves |= RAYS[i][nrt]; // north
    if (RAYS[i][nrt] & blockers) {
        blocker_index = bit_scan_left(RAYS[i][nrt] & blockers);
        moves &= (~RAYS[blocker_index][nrt]);
    }

    moves |= RAYS[i][sth]; // south
    if (RAYS[i][sth] & blockers) {
        blocker_index = bit_scan_right(RAYS[i][sth] & blockers);
        moves &= (~RAYS[blocker_index][sth]);
    }

    moves |= RAYS[i][est]; // east
    if (RAYS[i][est] & blockers) {
        blocker_index = bit_scan_right(RAYS[i][est] & blockers);
        moves &= ((~RAYS[blocker_index][est]));
    }

    moves |= RAYS[i][wst]; // west
    if (RAYS[i][wst] & blockers) {
        blocker_index = bit_scan_left(RAYS[i][wst] & blockers);
        moves &= (~RAYS[blocker_index][wst]);
    }

    return moves;
}

uint64_t gen_B_moves(int i) {
    // bishops move along diagonal rays if they are not blocked
    uint64_t moves = 0;
    int blocker_index;
    // TODO: understand why I had the following lines instead of just blockers = ~pos[empty]
    // uint64_t blockers;
    // if (pos[white] & (1ULL << i)) { blockers = ~(pos[empty] | pos[bK]); }
    // else { blockers = ~(pos[empty] | pos[wK]); }
    uint64_t blockers = ~pos[empty];

    moves |= RAYS[i][nrtest]; // north east
    if (RAYS[i][nrtest] & blockers) {
        blocker_index = bit_scan_left(RAYS[i][nrtest] & blockers); 
        moves &= (~RAYS[blocker_index][nrtest]);
    }

    moves |= RAYS[i][nrtwst]; // north west
    if (RAYS[i][nrtwst] & blockers) {
        blocker_index = bit_scan_left(RAYS[i][nrtwst] & blockers); 
        moves &= (~RAYS[blocker_index][nrtwst]);
    }

    moves |= RAYS[i][sthest]; // south east
    if (RAYS[i][sthest] & blockers) {
        blocker_index = bit_scan_right(RAYS[i][sthest] & blockers); 
        moves &= (~RAYS[blocker_index][sthest]);
    }

    moves |= RAYS[i][sthwst]; // south west
    if (RAYS[i][sthwst] & blockers) {
        blocker_index = bit_scan_right(RAYS[i][sthwst] & blockers); 
        moves &= (~RAYS[blocker_index][sthwst]);
    }

    return moves; 
}

uint64_t gen_Q_moves(int i) {
    // queens make any rook or bishop move
    return (gen_R_moves(i) | gen_B_moves(i));
}

uint64_t generate_piece_moves(int i) {
    if (pos[wP] & (1ULL << i)) { return gen_wP_moves(i); }
    else if (pos[bP] & (1ULL << i)) { return gen_bP_moves(i); }
    else if ((pos[wN] | pos[bN]) & (1ULL << i)) { return gen_N_moves(i); }
    else if ((pos[wB] | pos[bB]) & (1ULL << i)) { return gen_B_moves(i); }
    else if ((pos[wR] | pos[bR]) & (1ULL << i)) { return gen_R_moves(i); }
    else if ((pos[wQ] | pos[bQ]) & (1ULL << i)) { return gen_Q_moves(i); }
    else if ((pos[wK] | pos[bK]) & (1ULL << i)) { return gen_K_moves(i); }

    return 0;
}

uint64_t generate_piece_attacks(int i) {
    if (pos[wP] & (1ULL << i)) { return gen_wP_attks(i); }
    else if (pos[bP] & (1ULL << i)) { return gen_bP_attks(i); }
    else if ((pos[wN] | pos[bN]) & (1ULL << i)) { return gen_N_moves(i); }
    else if ((pos[wB] | pos[bB]) & (1ULL << i)) { return gen_B_moves(i); }
    else if ((pos[wR] | pos[bR]) & (1ULL << i)) { return gen_R_moves(i); }
    else if ((pos[wQ] | pos[bQ]) & (1ULL << i)) { return gen_Q_moves(i); }
    else if ((pos[wK] | pos[bK]) & (1ULL << i)) { return gen_K_moves(i); }

    return 0;
}

void generate_checks(int color) {
    checks[color-white] = 0;
    for (int i=63; i>=0; i--) {
        if (pos[color] & (1ULL << i)) {
            checks[color-white] |= generate_piece_attacks(i);
        }
    }
}

void update_checks() {
    generate_checks(white);
    generate_checks(black);
}

/////////////////////////////////////////////////////////////////////////////////////
// BOARD MANIPULATION
// methods for making and taking back moves
/////////////////////////////////////////////////////////////////////////////////////
void short_castle(int color, int depth) {
    if (color == white) {
        pos[wK] = (1ULL << 1);
        pos[wR] ^= (1ULL << 0);
        pos[wR] |= (1ULL << 2);
        capture_sequence[depth-1] = w_castle_short;
        is_castled_w = true;
    }

    else {
        pos[bK] = (1ULL << 57);
        pos[bR] ^= (1ULL << 56);
        pos[bR] |= (1ULL << 58);
        capture_sequence[depth-1] = b_castle_short;
        is_castled_b = true;
    }
    update_colors();
}

void long_castle(int color, int depth) {
    if (color == white) {
        pos[wK] = (1ULL << 5);
        pos[wR] ^= (1ULL << 7);
        pos[wR] |= (1ULL << 4);
        capture_sequence[depth-1] = w_castle_long;
        is_castled_w = true;
    }

    else {
        pos[bK] = (1ULL << 61);
        pos[bR] ^= (1ULL << 63);
        pos[bR] |= (1ULL << 60);
        capture_sequence[depth-1] = b_castle_long;    
        is_castled_b = true;    
    }
    update_colors();
}

void castle(int origination, int destination, int depth) {
    if ((origination == 3) && (destination == 1)) { short_castle(white, depth); }
    else if ((origination == 3) && (destination == 5)) { long_castle(white, depth); }
    else if ((origination == 59) && (destination == 57)) { short_castle(black, depth); }
    else { long_castle(black, depth); }
}

void promote(int origination, int destination, int depth) {
    for (int i=0; i<13; i++) {
        if (pos[i] & (1ULL << destination)) {
            pos[i] ^= (1ULL << destination);
            capture_sequence[depth-1] = (i + promotion_key);
            break;
        }
    }

    if (RANK_8 & (1ULL << destination)) {
        pos[wP] ^= (1ULL << origination);
        pos[wQ] |= (1ULL << destination);
    }

    else {
        pos[bP] ^= (1ULL << origination);
        pos[bQ] |= (1ULL << destination);
    }
    update_colors();
}

void make_move(int origination, int destination, int depth) {
    // TODO: Error - destination can be en_passant_b/w without being en passant move
    if (en_passant_b & (1ULL << destination)) {
        pos[wP] ^= (1ULL << origination);
        pos[wP] |= (1ULL << destination);
        pos[bP] ^= (1ULL << (destination-8));
        capture_sequence[depth-1] = (bP+en_passant_key);
    }
    else if (en_passant_w & (1ULL << destination)) {
        pos[bP] ^= (1ULL << origination);
        pos[bP] |= (1ULL << destination);
        pos[wP] ^= (1ULL << (destination+8));
        capture_sequence[depth-1] = (wP+en_passant_key);
    }

    else if (((pos[wK] | pos[bK]) & (1ULL << origination)) && (std::abs(origination - destination) == 2)) {
        castle(origination, destination, depth);
    }

    else if (((pos[wP] | pos[bP]) & (1ULL << origination)) && ((RANK_1 | RANK_8) & (1ULL << destination))) {
        promote(origination, destination, depth);
    }

    else {
        // remove enemy piece
        for (int pieceID=0; pieceID<13; pieceID++) {
            if (pos[pieceID] & (1ULL << destination)) {
                pos[pieceID] ^= (1ULL << destination);
                capture_sequence[depth-1] = pieceID;
                break;
            }
        }
        // move own piece
        for (int pieceID=0; pieceID<13; pieceID++) {
            if (pos[pieceID] & (1ULL << origination)) {
                pos[pieceID] ^= (1ULL << origination);
                pos[pieceID] |= (1ULL << destination);
                break;
            }
        }
    }

    en_passant_w = 0; en_passant_b = 0;
    // if ((pos[wP] & (1ULL << origination)) && ((destination-origination) == 16)) {
    //     en_passant_w = (1ULL << (origination+8));
    // }
    // else if ((pos[bP] & (1ULL << origination)) && ((origination-destination) == 16)) {
    //     en_passant_b = (1ULL << (origination-8));
    // }

    update_colors();
}

void takeback_move(int origination, int destination, int depth) {
    // castling
    if (capture_sequence[depth-1] == w_castle_short) {
        pos[wK] = (1ULL << 3);
        pos[wR] ^= (1ULL << 2);
        pos[wR] |= (1ULL << 0);
        is_castled_w = false;
    }
    else if (capture_sequence[depth-1] == w_castle_long) {
        pos[wK] = (1ULL << 3);
        pos[wR] ^= (1ULL << 4);
        pos[wR] |= (1ULL << 7);   
        is_castled_w = false;        
    }
    else if (capture_sequence[depth-1] == b_castle_short) {
        pos[bK] = (1ULL << 59);
        pos[bR] ^= (1ULL << 58);
        pos[bR] |= (1ULL << 56);
        is_castled_b = false;
    }
    else if (capture_sequence[depth-1] == b_castle_long) {
        pos[bK] = (1ULL << 59);
        pos[bR] ^= (1ULL << 60);
        pos[bR] |= (1ULL << 63);
        is_castled_b = false;
    }
    // en passant and promotion
    else if (capture_sequence[depth-1] >= 17) {
        if (pos[wQ] & RANK_8 & (1ULL << destination)) {
            pos[wQ] ^= (1ULL << destination);
            pos[wP] |= (1ULL << origination);
            pos[capture_sequence[depth-1] - promotion_key] |= (1ULL << destination);
        }
        else if (pos[bQ] & RANK_1 & (1ULL << destination)) {
            pos[bQ] ^= (1ULL << destination);
            pos[bP] |= (1ULL << origination);
            pos[capture_sequence[depth-1] - promotion_key] |= (1ULL << destination);
        }
        else if ((capture_sequence[depth-1] - en_passant_key) == bP) {
            pos[wP] ^= (1ULL << destination);
            pos[wP] |= (1ULL << origination);
            pos[bP] |= (1ULL << (destination-8));
            en_passant_b = (1ULL << destination);
        }
        else if ((capture_sequence[depth-1] - en_passant_key) == wP) {
            pos[bP] ^= (1ULL << destination);
            pos[bP] |= (1ULL << origination);
            pos[wP] |= (1ULL << (destination+8));
            en_passant_w = (1ULL << destination);
        }
    }
    // standard moves
    else {
        for (int i=0; i<13; i++) {
            if (pos[i] & (1ULL << destination)) {
                pos[i] ^= (1ULL << destination);
                pos[i] |= (1ULL << origination);
            }
        }
        pos[capture_sequence[depth-1]] |= (1ULL << destination);
    }
    update_colors();
}

/////////////////////////////////////////////////////////////////////////////////////
// MOVE ORDER HEURISTICS
/////////////////////////////////////////////////////////////////////////////////////
void update_principle_variation(int color) {
    // uint64_t key = gen_zobrist_key(color);
    // int hash = gen_hash_index(key);
    // for (int n=0; n<iteration_depth; n++) {
    //     // retrieve best move from transposition table
    //     int best_move = HASH_TABLE[hash][p_best];
    //     principal_variation[n] = best_move;
    //     int origination = (best_move >> 6);
    //     int destination = (best_move & 63);
    //     // find zobrist key for next position
    //     make_move(origination, destination, n+1);
    //     color = opp(color);
    //     key = gen_zobrist_key(color);
    //     hash = gen_hash_index(key);
    // }
    // // undo board manipulation
    // for (int n=iteration_depth; n>0; n--) {
    //     int origination = (principal_variation[n-1] >> 6);
    //     int destination = (principal_variation[n-1] & 63);
    //     takeback_move(origination, destination, n);
    // }
}

int mvv_lva(int move, int color) {
    // rank captures by most valuable victim, least valuable attacker
    int origination = (move >> 6);
    int destination = (move & 63);
    int eval = 0;

    return (material_value_at(destination) - material_value_at(origination));
}

int heuristic_eval(int move, int color, int depth) {
    int origination = (move >> 6);
    int destination = (move & 63);
    int eval = 0;
    // principal variation move
    if (move == principal_variation[iteration_depth - depth]) { eval += 3000; }

    // previous branch best move
    if (move == layer_best_moves[iteration_depth - depth]) { eval += 2000; }

    // killer moves
    if (move == layer_killer_moves[depth]) { eval += 1000; }

    // transposition table move
    //int key = gen_zobrist_key(color);
    //int hash = gen_hash_index(key);
    //if ((key == HASH_TABLE[hash][p_key]) && (move == HASH_TABLE[hash][p_best])) { eval += 10000; }

    // checks
    make_move(origination, destination, 2*MAX_DEPTH+120);
    generate_checks(color);
    if ((pos[wK] | pos[bK]) & ~pos[color] & checks[color-white]) { eval += 500; }    // add incentive for searching checks
    takeback_move(origination, destination, 2*MAX_DEPTH+120);
    generate_checks(color);

    // captures
    if (pos[opp(color)] & (1ULL << destination)) {
        eval += 150;    // static incentive
        eval += mvv_lva(move, color);   // mvv-lva ordering
    }

    // putting pawns and knights in center
    if ((1ULL << origination) & (pos[wP] | pos[bP] | pos[wN] | pos[bN]) & pos[color]) {
        if ((1ULL << destination) & MIDDLE) { eval += MIDDLE_BONUS; }
        else if ((1ULL << destination) & AUXMID) { eval += AUXMID_BONUS; }
        else if ((1ULL << destination) & EDGE) { eval -= EDGE_PENALTY; }
    }

    // incentivize moving minor pieces ahead of K,Q,pawns
    int material_val = material_value_at(origination);
    if (material_val == B_VAL || material_val == N_VAL) { eval += 10; }

    // average previous node board evals
    // eval += layer_previous_evals[iteration_depth-depth][move];
    // eval /= 2;


    return eval;
}

/////////////////////////////////////////////////////////////////////////////////////
// MOVES LIST
// functions for generating and ordering the list of possible moves at a given depth
/////////////////////////////////////////////////////////////////////////////////////
int generate_color_moves_list(int color, int depth) {
    uint64_t moves = 0;
    int counter = 0;
    // scanning board in reverse gives slightly better move ordering since kingside is on right
    for (int i=63; i>=0; i--) {
        if (pos[color] & (1ULL << i)) {
            moves = generate_piece_moves(i);
            moves &= ~pos[color];

            // locate destinations
            for (int j=0; j<64; j++) {
                if (moves & (1ULL << j)) {
                    int move = ((i << 6) | j);
                    moves_list[depth-1][counter] = move;
                    counter++;
                }
            }
        }
    }
    return counter;
}

void score_list(int num_moves, int color, int depth) {
    for (int i=0; i<num_moves; i++) {
        values_list[depth-1][i] = heuristic_eval(moves_list[depth-1][i], color, depth);
    }
}

int generate_color_attks_list(int color, int depth) {
    uint64_t attks = 0;
    int counter = 0;
    // scanning board in reverse gives slightly better move ordering since kingside is on right
    for (int i=63; i>=0; i--) {
        if (pos[color] & (1ULL << i)) {
            attks = generate_piece_moves(i);
            attks &= pos[opp(color)];

            // locate destinations
            for (int j=0; j<64; j++) {
                if (attks & (1ULL << j)) {
                    int move = ((i << 6) | j);
                    moves_list[depth-1][counter] = move;
                    counter++;
                }
            }
        }
    }
    return counter;
}

void score_captures(int num_moves, int color, int depth) {
    for (int i=0; i<num_moves; i++) {
        values_list[depth-1][i] = mvv_lva(moves_list[depth-1][i], color);
    }
}

int get_next_best_move(int move_num, int num_moves, int depth) {
    // find the next highest score and shift it to next spot on list
    int best_score = -2000000;
    int best_index = move_num;
    for (int i=move_num; i<num_moves; i++) {
        if (values_list[depth-1][i] > best_score) {
            best_score = values_list[depth-1][i];
            best_index = i;
        }
    }
    int buffer = values_list[depth-1][best_index];
    values_list[depth-1][best_index] = values_list[depth-1][move_num];
    values_list[depth-1][move_num] = buffer;

    buffer = moves_list[depth-1][best_index];
    moves_list[depth-1][best_index] = moves_list[depth-1][move_num];
    moves_list[depth-1][move_num] = buffer;

    return buffer;
}

/////////////////////////////////////////////////////////////////////////////////////
// BOARD EVALUATION
// the engine looks at a combination of material and positional advantages and also
// orders each move with heuristic weight to improve alpha-beta cut off rates
/////////////////////////////////////////////////////////////////////////////////////
bool game_is_won_by_checkmate() {
    update_checks();
    if (pos[wK] & checks[1]) {
        int num_moves = generate_color_moves_list(white, 2*MAX_DEPTH+1);
        bool in_checkmate = true;
        for (int i=0; i<num_moves; i++) {
            int origination = (moves_list[2*MAX_DEPTH][i] >> 6);
            int destination = (moves_list[2*MAX_DEPTH][i] & 63);
            make_move(origination, destination, 2*MAX_DEPTH+1);
            generate_checks(black);
            takeback_move(origination, destination, 2*MAX_DEPTH+1);
            if ((pos[wK] & checks[1]) == 0) {
                in_checkmate = false;
                break;
            }
        }
        return in_checkmate;
    }
    else if (pos[bK] & checks[0]) {
        int num_moves = generate_color_moves_list(black, 2*MAX_DEPTH+1);
        bool in_checkmate = true;
        for (int i=0; i<num_moves; i++) {
            int origination = (moves_list[2*MAX_DEPTH][i] >> 6);
            int destination = (moves_list[2*MAX_DEPTH][i] & 63);
            make_move(origination, destination, 2*MAX_DEPTH+1);
            generate_checks(white);
            takeback_move(origination, destination, 2*MAX_DEPTH+1);
            if (pos[wK] & ~checks[0]) {
                in_checkmate = false;
                break;
            }
        }
        return in_checkmate;
    }
    else { return false; }
}

bool game_is_drawn_by_insufficient_material() {
    // K vs K
    if ((pos[white] == pos[wK]) && (pos[black] == pos[bK])) { return true; }
    // K+N vs K
    else if ((pos[white] == (pos[wK] | pos[wN])) && (pos[black] == pos[bK]) 
            && (count(pos[white]) == 2) && (count(pos[black]) == 1)) { return true; }
    // K vs K+N
    else if ((pos[white] == pos[wK]) && (pos[black] == (pos[bK] | pos[bN]))
            && (count(pos[white]) == 1) && (count(pos[black]) == 2)) { return true; }
    // K+B vs K
    else if ((pos[white] == (pos[wK] | pos[wB])) && (pos[black] == pos[bK])
            && (count(pos[white]) == 2) && (count(pos[black]) == 1)) { return true; }
    // K vs K+B
    else if ((pos[white] == pos[wK]) && (pos[black] == (pos[bK] | pos[bB]))
            && (count(pos[white]) == 1) && (count(pos[black]) == 2)) { return true; }
    // K+N vs K+B
    else if ((pos[white] == (pos[wK] | pos[wN])) && (pos[black] == (pos[bK] | pos[bB]))
            && (count(pos[white]) == 2) && (count(pos[black]) == 2)) { return true; }
    // K+B vs K+N
    else if ((pos[white] == (pos[wK] | pos[wB])) && (pos[black] == (pos[bK] | pos[bN]))
            && (count(pos[white]) == 2) && (count(pos[black]) == 2)) { return true; }
    // K+B vs K+B
    else if ((pos[white] == (pos[wK] | pos[wB])) && (pos[black] == (pos[bK] | pos[bB]))
            && (count(pos[white]) == 2) && (count(pos[black]) == 2)) { return true; }
    // K+N vs K+N
    else if ((pos[white] == (pos[wK] | pos[wN])) && (pos[black] == (pos[bK] | pos[bN]))
            && (count(pos[white]) == 2) && (count(pos[black]) == 2)) { return true; }
    // sufficient material
    else { return false; }
}

int node_evaluation() {
    int evaluation = 0;
    for (int i=0; i<64; i++) {
        if (pos[wP] & (1ULL << i)) { evaluation += (PST[wP][63-i] + VAL[wP]); }
        else if (pos[wN] & (1ULL << i)) { evaluation += (PST[wN][63-i] + VAL[wN]); }
        else if (pos[wB] & (1ULL << i)) { evaluation += (PST[wB][63-i] + VAL[wB]); }
        else if (pos[wR] & (1ULL << i)) { evaluation += (PST[wR][63-i] + VAL[wR]); }
        else if (pos[wQ] & (1ULL << i)) { evaluation += (PST[wQ][63-i] + VAL[wQ]); }
        else if (pos[wK] & (1ULL << i)) { 
            if (count(pos[black] ^ (pos[bK] | pos[bP])) <= ENDGAME_CUTOFF) { evaluation += (PST[wK+1][63-i] + VAL[wK]); }
            else { evaluation += (PST[wK][63-i] + VAL[wK]); }
        }

        else if (pos[bP] & (1ULL << i)) { evaluation -= (PST[wP][63-black_pst_index(i)] + VAL[wP]); }
        else if (pos[bN] & (1ULL << i)) { evaluation -= (PST[wN][63-black_pst_index(i)] + VAL[wN]); }
        else if (pos[bB] & (1ULL << i)) { evaluation -= (PST[wB][63-black_pst_index(i)] + VAL[wB]); }
        else if (pos[bR] & (1ULL << i)) { evaluation -= (PST[wR][63-black_pst_index(i)] + VAL[wR]); }
        else if (pos[bQ] & (1ULL << i)) { evaluation -= (PST[wQ][63-black_pst_index(i)] + VAL[wQ]); }
        else if (pos[bK] & (1ULL << i)) {
            if (count(pos[white] ^ (pos[wK] | pos[wP])) <= ENDGAME_CUTOFF) { evaluation -= (PST[wK+1][black_pst_index(63-i)] + VAL[wK]); }
            else { evaluation -= (PST[wK][black_pst_index(63-i)] + VAL[wK]); }
        }
    }
    evaluation += (MOBILITY_INCENTIVE * (count(checks[0]) - count(checks[1])));
    return evaluation;
}

/////////////////////////////////////////////////////////////////////////////////////
// MOVE SEARCH
// the engine itself - a core negamax implementation of the minimax algorithm with 
// alpha-beta pruning.  Iterative deepening, a variety of move ordering heuristics,
// and a transposition table are used to improve alpha-beta cut off rates.
/////////////////////////////////////////////////////////////////////////////////////
int quiescence_search(int color, int depth, int alpha, int beta) {
    q_nodes++;
    max_quiescence_search_depth = std::max(max_quiescence_search_depth, depth);
    int num_moves = generate_color_attks_list(color, iteration_depth+depth+1);
    int static_eval = node_evaluation();
    if (color == black) { static_eval *= -1; }

    // standing eval cut off
    if (static_eval >= beta) {
        return beta; }
    if (static_eval > alpha) { alpha = static_eval; }

    int best_eval = static_eval; int eval;
    if ((num_moves == 0) || (depth >= (Q_EXPANSION_FACTOR*iteration_depth))) {
        return static_eval;
    }
    // end search if either king is captured
    else if ((pos[wK] == 0) || (pos[bK] == 0)) { return -10000; }

    // evade checks only at surface depth
    else if (depth == 0) {
        if (pos[wK] & checks[1] || (pos[bK] & checks[0])) {
            int num_moves = generate_color_moves_list(color, iteration_depth+depth+1);
            for (int i=0; i<num_moves; i++) {
                int origination = (moves_list[iteration_depth+depth][i] >> 6);
                int destination = (moves_list[iteration_depth+depth][i] & 63);
                uint64_t savepoint = pos[empty];
                make_move(origination, destination, iteration_depth+depth+1);
                update_checks();
                if ((pos[wK] & checks[1]) || (pos[bK] & checks[0])) { 
                    takeback_move(origination, destination, iteration_depth+depth+1);
                }
                else {
                    eval = -quiescence_search(opp(color), depth+1, -beta, -alpha);
                    takeback_move(origination, destination, iteration_depth+depth+1);

                    best_eval = std::max(eval, best_eval);
                    alpha = std::max(alpha, best_eval);
                    if (alpha >= beta) { break; }
                }
            }
        }
    }

    else {
        score_captures(num_moves, color, iteration_depth+depth+1);
        for (int i=0; i<num_moves; i++) {
            int move = get_next_best_move(i, num_moves, iteration_depth+depth+1);
            int origination = (move >> 6); int destination = (move & 63);
            uint64_t savepoint = pos[empty];
            make_move(origination, destination, iteration_depth+depth+1);
            update_checks();
            eval = -quiescence_search(opp(color), depth+1, -beta, -alpha);
            takeback_move(origination, destination, iteration_depth+depth+1);
            best_eval = std::max(eval, best_eval);
            alpha = std::max(alpha, best_eval);
            if (alpha >= beta) { break; }
        }
    }
    // no good captures were found
    return best_eval;
}

int minimax(int color, int depth, int terminal_depth, int alpha, int beta) {
    if (depth == terminal_depth) {
        if (pos[opp(color)] & checks[color-white]) {
            int eval = quiescence_search(color, 0, alpha, beta);
            // bc quiescence forces captures, only accept evals that indicate instability
            if (std::abs(eval) > STABILITY_WINDOW) { return eval; }
        }
        // node eval is oriented toward white
        if (color == white) { return node_evaluation(); }
        else { return -node_evaluation(); }
    }

    // king capture
    else if ((pos[wK] && pos[bK]) == 0) { return -1000000; }

    // textbook draw
    else if (game_is_drawn_by_insufficient_material()) {
        if (color == white) { return -DRAW_PENALTY; }
        else { return DRAW_PENALTY; }
    }

    int best_move; int best_eval;
    uint64_t orig_pos_key = gen_zobrist_key(color);
    int orig_pos_hash = gen_hash_index(orig_pos_key);

    // if position has already been searched to the same depth or better, use that evaluation
    if ((HASH_TABLE[orig_pos_hash][p_key] == orig_pos_key) && (HASH_TABLE[orig_pos_hash][p_depth] >= depth) && (depth > 1)) { 
        // best move only listed if depth > 1
        // depth should be at least one greater than current depth bc entries are stored at move_pos_key, not orig_pos_key, which is one depth higher
        hash_entries++;
        best_move = HASH_TABLE[orig_pos_hash][p_best];
        best_eval = HASH_TABLE[orig_pos_hash][p_eval];
    }

    // otherwise use minimax algorithm to explore move tree
    else {
        // clear previous branch
        for (int i=0; i<depth; i++) { current_variation[iteration_depth - i] = 0; }
        int board_eval; best_eval = -2000000;

        int num_moves = generate_color_moves_list(color, depth);
        score_list(num_moves, color, depth);

        for (int i=0; i<num_moves; i++) {
            // late move reduction
            int updated_terminal_depth = terminal_depth;
            if ((depth >= (terminal_depth + 3)) && (i > 1)) {
                if (i > (num_moves * 0.70)) {
                    late_move_reductions++;
                    updated_terminal_depth = terminal_depth + 2;
                }
                else if (i > (num_moves * 0.30)) {
                    late_move_reductions++;
                    updated_terminal_depth = terminal_depth + 1;
                }
            }
            // play next move
            int move = get_next_best_move(i, num_moves, depth);
            current_variation[iteration_depth - depth] = move;
            int origination = (move >> 6); int destination = (move & 63);
            make_move(origination, destination, depth);

            // skip move if it fails to prevent check
            update_checks();
            if (((color == white) && (pos[wK] & checks[1])) || 
                ((color == black) && (pos[bK] & checks[0]))) {
                takeback_move(origination, destination, depth);
            }
            
            else {
                m_nodes++;
                uint64_t move_pos_key = gen_zobrist_key(opp(color)); // the position arising after a move is the other player's turn
                int move_pos_hash = gen_hash_index(move_pos_key);

                // go to next depth
                board_eval = -minimax(opp(color), depth-1, updated_terminal_depth, -beta, -alpha);
                takeback_move(origination, destination, depth);
                layer_previous_evals[iteration_depth-depth][move] = board_eval;

                // store position attributes in hash table
                HASH_TABLE[move_pos_hash][p_key] = move_pos_key;
                HASH_TABLE[move_pos_hash][p_eval] = board_eval;
                HASH_TABLE[move_pos_hash][p_depth] = depth-1; // the analysis for this move doesn't actually start until the next depth

                // update best move selection
                if (board_eval > best_eval) {
                    best_eval = board_eval;
                    best_move = moves_list[depth-1][i];
                }

                // test for alpha-beta cut off
                alpha = std::max(alpha, best_eval);
                if (alpha >= beta) {
                    cut_offs++;
                    layer_killer_moves[depth] = moves_list[depth-1][i];
                    break;
                }
            }
        }
        // use minimax findings to update best move in original position
        HASH_TABLE[orig_pos_hash][p_best] = best_move;
    }

    layer_best_moves[iteration_depth-depth] = best_move;
    if (depth == iteration_depth) { return best_move; }
    else { return best_eval; }
}


int depth_search(int color, int depth_cap, bool printout=false) {
    // TODO: end depth search if forced mate found early
    iteration_depth = 1;
    int move;
    clock_t start = time(0);
    // while (((time(0) - start) < MAX_SEARCH_TIME) && (iteration_depth <= depth_cap)) {
    while (iteration_depth <= depth_cap) {
        // clear engine statistics from previous iteration
        m_nodes = 0; cut_offs = 0; late_move_reductions = 0; hash_entries = 0;
        q_nodes = 0; max_quiescence_search_depth = 0; called = 0;

        // run current iteration
        auto init = std::chrono::high_resolution_clock::now();
        move = minimax(color, iteration_depth, 0, -2000000, 2000000);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - init);
        timer2 += duration.count();

        total_nodes += (m_nodes+q_nodes);

        //std::cout << gen_zobrist_key(color) << std::endl;
        update_principle_variation(color);

        // print statistics
        if (printout) {
            // print coordinates
            print_coords(move);

            // print eval
            std::cout << "  reached depth " << iteration_depth << " in " << (time(0)-start) << " seconds" << std::endl;
            std::cout << m_nodes << " minimax nodes";
            std::cout << ", " << q_nodes << " quiesce nodes";
            std::cout << ", " << total_nodes << " total nodes";
            std::cout << ", " << cut_offs << " cut offs";
            std::cout << ", " << late_move_reductions << " reductions";
            std::cout << ", " << hash_entries << " hashes";
            std::cout << ",  max q depth: " << max_quiescence_search_depth << std::endl;

            std::cout << "best cont: ";
            for (int i=0; i<iteration_depth; i++) {
                int pv = principal_variation[i];
                
                // print coordinates
                print_coords(pv);
                std::cout << ", ";
            }
            std::cout << "\n\n";
        }

        iteration_depth++;
    }
    return move;
}

// Main lichess bot
// int main(int argc, char* argv[]) {
int main() {
    std::ofstream outfile("eval.txt");
    outfile.close();
    fill_RAYS(); update_checks(); seed_tables();

    // for (int i=0; i<argc; i++) {
    //     if (std::string(argv[i]) == "-i") {
    //         read_FEN(argv[++i]);
    //     }
    // }
    
    // make_move(12,28,1);
    // make_move(52,36,2);
    // for (int lyr=0; lyr<MAX_DEPTH; lyr++) {
    //     std::cout << capture_sequence[lyr] << std::endl;
    // }
    read_FEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    int move = depth_search(computer, MAX_DEPTH, true);
    print_coords(move);

    return 0;
}