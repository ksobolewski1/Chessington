#pragma once
#include <unordered_map>
#include <memory>
#include "Move.h"

struct PieceGroup
{
private:
	uint64_t short_castle_mask;
	uint64_t long_castle_mask;

	uint64_t kingside_rook_mask;
	uint64_t queenside_rook_mask;

	// xor values 
	unsigned long long int rook_king_corner; 
	unsigned long long int rook_queen_corner;
	unsigned long long int rook_castled_short;
	unsigned long long int rook_castled_long;

	int material_map[12] = {10, 30, 35, 50, 90, 0, 10, 30, 35, 50, 90, 0};

	void UpdatePiecePosition(int old_sqr, int new_sqr, uint64_t mask);

	void CountMaterial();

public:

	std::unordered_map<int, int> map;
	uint64_t group_bitboard = 0; 
	uint64_t king_mask = 0;
	int king_sqr = 0;
	int castling_rights = 0;
	int material = 0; 
	int piece_material = 0; 

	PieceGroup() = default;
	PieceGroup(const std::vector<int>& board, int colour, int castle, 
		unsigned long long int r_q_c, unsigned long long int r_k_c, unsigned long long int r_c_s, unsigned long long int r_c_l);

	void AddPiece(int sqr, int id, uint64_t mask);
	void RemovePiece(int square, uint64_t mask);
	void UpdatePiecePosition(int old_sqr, int new_sqr, uint64_t old_mask, uint64_t new_mask);

	int GetPieceID(int sqr);

	void Promote(int sqr, int new_id);

	void CastleShort(unsigned long long int& hash);
	void CastleLong(unsigned long long int& hash);
	void UndoCastleShort();
	void UndoCastleLong();

	int PawnCount();
	int KnightCount();
};

