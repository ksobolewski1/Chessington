#pragma once
#include <random>
#include <thread>
#include <mutex>
#include <fstream>
#include <string>
#include <cstdlib>
#include <map>
#include <chrono>

#include "PieceGroup.h"
#include "EvalTables.h"

////// Chessington-Enginegton communication

	// both processes write to "enginegton_log.txt"

	// 1. "get" - get all legal moves in a given position 
	// 2. "find" - find a move in a given position 
	// 3. "stop" - ask the engine to stop searching
	// 4. "term" - ask the engine process to close 

class Enginegton
{
private:

	std::unordered_map<char, int> fen_read = {
		{'0', -1}, {'P', 0}, {'p', 6}, {'N', 1}, {'n', 7}, {'B', 2}, {'b', 8}, {'R', 3}, {'r', 9}, {'Q', 4}, {'q', 10}, {'K', 5}, {'k', 11} };

	std::unordered_map<std::string, int> requests = {{"get", 0}, {"find", 1}, {"stop", 2}, {"term", 3}};

	std::string path_to_log;
	std::string private_log_path; // for logging data on the last search 
	std::string private_log_board; // store the board for the private log 

	void CheckLog();
	std::vector<std::string> GetReadOut();
	void Post(std::string res);
	void WaitForEmptyLog();
	void ParseRequest(std::string& b, std::string& t, std::string& m, std::string& c);
	void PrivateLog(Move& move, unsigned int table_s, float eval, float nps);

	bool terminate = false;
	bool stop_search = false; 
	bool timeout = false; 

	float max_search_time = 30000.0f;

	std::string request = "";
	int orientation = 1;

	void SearchThread();
	bool searching = false;
	
	void PostMove(Move& move);
	void Flip(Move& move);

	////// Main functions to call in response to Chessington requests 
	void GetMoves(std::vector<std::string>& r_out);
	void FindMove(std::vector<std::string>& r_out);
	
	////// Masks and bitboards 
	uint64_t origin_masks[64]; 
	uint64_t white_pawn_control_bitboards[64];
	uint64_t black_pawn_control_bitboards[64];
	uint64_t white_pawn_push_bitboards[64];
	uint64_t black_pawn_push_bitboards[64];
	uint64_t knight_control_bitboards[64];
	uint64_t king_control_bitboards[64];
	uint64_t king_areas[64];
	// unrestricted rook and bishop controls, including edge squares 
	uint64_t rook_control_bitboards[64]; 
	uint64_t bishop_control_bitboards[64]; 
	// masks for rook and bishop control, without edge squares 
	uint64_t rook_masks[64];
	uint64_t bishop_masks[64];

	// castling masks 
	const uint64_t white_long_castle_bitboard = 0b0000111000000000000000000000000000000000000000000000000000000000; 
	const uint64_t white_short_castle_bitboard = 0b0110000000000000000000000000000000000000000000000000000000000000;
	const uint64_t white_long_sub_bitboard = 0b0000110000000000000000000000000000000000000000000000000000000000; 
	const uint64_t black_long_castle_bitboard = 0b0000000000000000000000000000000000000000000000000000000000001110;
	const uint64_t black_short_castle_bitboard = 0b0000000000000000000000000000000000000000000000000000000001100000;
	const uint64_t black_long_sub_bitboard = 0b0000000000000000000000000000000000000000000000000000000000001100;

	uint64_t en_passant_mask = 0; 

	/// Storing the indices of the uint64_t bits for each square, for use in BitScan() methods, to ensure only possible move-bits of the uint_64 are checked
	std::vector<int> w_pawn_scan_indices[64];
	std::vector<int> b_pawn_scan_indices[64];
	std::vector<int> king_scan_indices[64];
	std::vector<int> knight_scan_indices[64];
	std::vector<int> bishop_scan_indices[64];
	std::vector<int> rook_scan_indices[64];

	////// Search 
	unsigned long long int pos_count = 0;
	unsigned long long int total_node_count; 
	unsigned long long int checkmate_count = 0;
	bool early_stop = false; // if checkmate-in-one is found, this variable stops the search 

	int max_depth = 6;

	std::vector<int> capture_log; // undo captures 
	std::vector<int> castle_log; // undo castling; both groups' castling rights are stored because there might be cases when both sides lose castling rights simultaneously
	// (one side's rook captures another's rook, with both at their starting positions), 
	// or when one side loses castling rights passively (e.g, when the opponent takes them away by capturing a rook)
	std::vector<unsigned long long int> hash_log; // undo hash 
	std::unordered_map<unsigned long long int, float> transposition_table; // to avoid analysing positions twice 

	float Calculate(int depth, const Move& move, float alpha, float beta); // main recursive function 
	// helpers 
	void ProcessMove(const Move& move);
	void UndoMove(const Move& move);
	int Promote(int in);
	int Unpromote();

	// random numbers for getting the zobrist hash for a position 
	unsigned long long int zob_table[64][12]; // for each piece on each square 
	unsigned long long int zob_castle[16]; // for each combination of castling rights 
	unsigned long long int zob_en_passant[8]; // for each file on which an en passant might be legal 
	unsigned long long int zob_turn; // if it is black's turn 

	////// Pieces 
	int turn = 1;
	uint64_t board_state_bitboard = 0;
	PieceGroup white_pieces;
	PieceGroup black_pieces;

	inline PieceGroup& GetPieces(int turn) {
		if (turn == 1) return white_pieces;
		else return black_pieces;
	}
	
	////// Move generation 
	uint64_t control = 0;
	uint64_t check_map = 0;
	std::unordered_map<int, uint64_t> pinned_pieces;
	int check = 0;
	bool checkmate = false; 
	bool stalemate = false;

	void MovesGen(MoveQueue& queue, bool initial=false); // the initial MovesGen happens before tree search, so in the future, its move ordering can be much more detailed 
	// the initial parameter is passed down to BitScan functions 
	void WaitingSide(int phase); // this function gathers evaluation data on the pieces that moved leading up to max depth; it builds control ahead of MovingSide()
	void MovingSide(); // this function returns once at least one move is found for the side to move at max depth; 
	// else, it sets checkmate/stalemate to true
	// this allows to skip bit scans on the final depth, leaving computational room for more elaborate evaluation functions to come 

	/// Rook and bishop magics
	uint64_t rook_magic_bitboards[64][4096];
	uint64_t bishop_magic_bitboards[64][512];
	unsigned long long int rook_magics[64];
	unsigned long long int bishop_magics[64];

	/// Getting all configurations of attacks from all configurations of blockers 
	void GetMagics(int origin, bool rook);

	/// Getting rook and bishop control using magics, including pins and checks 
	inline uint64_t RookControl(int origin, uint64_t king_mask, int king_sqr);
	inline uint64_t BishopControl(int origin, uint64_t king_mask, int king_sqr);
	// Getting the same control but without checking for checks or pins 
	inline uint64_t RookControl(int origin);
	inline uint64_t BishopControl(int origin);
	// Finds the index of the common blocker (a candidate for pin) between a sliding piece and the opposing king; returns -1 if more than one blocker was found 
	int RayIndex(uint64_t mask, int start); 
	// Rook and bishop moves using magics 
	inline uint64_t RookMoves(int origin, uint64_t own_pieces);
	inline uint64_t BishopMoves(int origin, uint64_t own_pieces);

	/// Bit scans - iterating over the possible moves and rating them 
	void WhitePawnBitScan(int origin, uint64_t moves, MoveQueue& queue);
	void BlackPawnBitScan(int origin, uint64_t moves, MoveQueue& queue);
	void KnightBitScan(int origin, uint64_t moves, MoveQueue& queue);
	void WhiteKingBitScan(int origin, uint64_t moves, MoveQueue& queue, int castle=0, uint64_t block=0);
	void BlackKingBitScan(int origin, uint64_t moves, MoveQueue& queue, int castle = 0, uint64_t block = 0);
	void BishopBitScan(int origin, uint64_t moves, MoveQueue& queue);
	void RookBitScan(int origin, uint64_t moves, MoveQueue& queue);
	void QueenBitScan(int origin, uint64_t moves, MoveQueue& queue);

	// counting bits with specified indices to check 
	int BitCount(uint64_t bits, const std::vector<int>& indices);
	// counting bits from a specified index 
	int BitCount(uint64_t bits, int from);

	////// Evaluation 
	const int backrank[8] = { 3, 1, 2, 4, 5, 2, 1, 3 }; // for checking the development status 

	int GetDevelopment(int col);
	int GetInitialGamePhase(); // happens ahead of the tree search, so can be more detailed in the future 
	int GetGamePhase();

	int initial_phase = 0; 
	int white_pieces_eval = 0;
	int black_pieces_eval = 0;
	float white_development = 0.0f;
	float black_development = 0.0f;
	float max_pieces_eval = 0.0f;
	const float max_dev = 1 / 8.0f;

	void EvaluateMovingSide(int phase); // if the game is not over, this gathers evaluation data on the side to move at max depth 
	float Evaluate(int phase);

	////// Move ordering 
	const int capture_values[12] = { 100, 200, 250, 350, 500, 0, 100, 200, 250, 350, 500, 0 };
	// multipliers allow to value a capture higher, the lesser the capturing piece's value;  
	// i.e., capturing a rook with a pawn is better than capturing with a queen, as far as a piece exchange, or making attacking threats, is concerned 
	const float capture_multipliers[12] = { 1.5, 0.9, 0.8, 0.7, 0.5, 1.0, 1.5, 0.9, 0.8, 0.7, 0.5, 1.00 }; 
	
	int RateKingMove(int dest);
	int RatePawnMove(int dest);
	int RateKnightMove(int dest);
	int RateBishopMove(int dest);
	int RateRookMove(int dest);
	int RateQueenMove(int dest);

public:

	Enginegton(const char* path, const char* priv_path);
	void Run();
};

