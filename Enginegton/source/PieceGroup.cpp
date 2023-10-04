#include "pch.h"
#include "PieceGroup.h"

// Castling rights - none if 0, long only if 1, short only if 2, both if 3 
PieceGroup::PieceGroup(const std::vector<int>& board, int colour, int castle, 
	unsigned long long int r_q_c, unsigned long long int r_k_c, unsigned long long int r_c_s, unsigned long long int r_c_l)
	: castling_rights(castle), rook_king_corner(r_k_c), rook_queen_corner(r_q_c), rook_castled_short(r_c_s), rook_castled_long(r_c_l)  {

	for (int i = 0; i < 64; ++i) {
		if ((colour == 1 && board[i] > -1 && board[i] < 6) || (colour == -1 && board[i] > 5)) {
			map[i] = board[i];
			group_bitboard |= (1ULL << i);
			if (board[i] == 5 || board[i] == 11) {
				king_mask = 1ULL << i;
				king_sqr = i;
			}
			material += material_map[board[i]];
			if (board[i] != 0 && board[i] != 6) piece_material += material_map[board[i]];
		}
	}

	// generate masks for castling 
	if (colour == 1) {
		kingside_rook_mask = 1ULL << 63;
		queenside_rook_mask = 1ULL << 56;
		short_castle_mask = kingside_rook_mask | (1ULL << 61);
		long_castle_mask = queenside_rook_mask | (1ULL << 59);
	}
	else {
		kingside_rook_mask = 1ULL << 7;
		queenside_rook_mask = 1ULL;
		short_castle_mask = kingside_rook_mask | (1ULL << 5);
		long_castle_mask = queenside_rook_mask | (1ULL << 3);
	}
}

void PieceGroup::AddPiece(int sqr, int id, uint64_t mask) { 

	map[sqr] = id; 
	group_bitboard |= mask; 
	material += material_map[id];

	if (id != 0 && id != 6) piece_material += material_map[id];
}

void PieceGroup::RemovePiece(int square, uint64_t mask) {
	group_bitboard ^= mask;
	int id = map[square];
	map.erase(square);
	material -= material_map[id];

	if (id != 0 && id != 6) piece_material -= material_map[id];

	if ((id == 3 || id == 9) && castling_rights > 0) {
		if (mask == kingside_rook_mask) castling_rights -= 2;
		else if (mask == queenside_rook_mask) castling_rights -= 1;
	}
}

void PieceGroup::UpdatePiecePosition(int old_sqr, int new_sqr, uint64_t mask) {

	group_bitboard ^= mask;

	int id = map[old_sqr];
	map.erase(old_sqr);
	map[new_sqr] = id;
}

void PieceGroup::UpdatePiecePosition(int old_sqr, int new_sqr, uint64_t old_mask, uint64_t new_mask) {

	group_bitboard ^= old_mask;
	group_bitboard |= new_mask;

	int id = map[old_sqr];
	map.erase(old_sqr);
	map[new_sqr] = id;

	if ((id == 3 || id == 9) && castling_rights > 0) {
		if (old_mask == kingside_rook_mask) castling_rights -= 2;
		else if (old_mask == queenside_rook_mask) castling_rights -= 1;
	}
	if (id == 5 || id == 11) {
		king_mask = new_mask;
		king_sqr = new_sqr;
		castling_rights = 0; 
	}
}

int PieceGroup::GetPieceID(int sqr) {

	if (map.count(sqr) > 0) return map[sqr];
	else return -1; 
}

void PieceGroup::Promote(int sqr, int new_id) {
	material -= material_map[map[sqr]];
	map[sqr] = new_id;
	material += material_map[new_id];
	if (new_id != 0 && new_id != 6) piece_material += material_map[new_id];
}

void PieceGroup::CastleShort(unsigned long long int& hash) {

	UpdatePiecePosition(king_sqr + 3, king_sqr + 1, short_castle_mask);
	hash ^= rook_king_corner;
	hash ^= rook_castled_short;
}

void PieceGroup::CastleLong(unsigned long long int& hash) {

	UpdatePiecePosition(king_sqr - 4, king_sqr - 1, long_castle_mask);
	hash ^= rook_queen_corner;
	hash ^= rook_castled_long;
}

void PieceGroup::UndoCastleShort() {
	UpdatePiecePosition(king_sqr - 1, king_sqr + 1, short_castle_mask);
}

void PieceGroup::UndoCastleLong() {
	UpdatePiecePosition(king_sqr + 1, king_sqr - 2, long_castle_mask);
}

void PieceGroup::CountMaterial() {
	for (const auto& [sqr, id] : map) material += material_map[id];
}

int PieceGroup::PawnCount() {
	int c = 0;
	for (const auto& [sqr, id] : map) {
		if (id == 0 || id == 6) ++c;
	}
	return c;
}

int PieceGroup::KnightCount() {
	int c = 0;
	for (const auto& [sqr, id] : map) {
		if (id == 1 || id == 7) ++c;
	}
	return c;
}
