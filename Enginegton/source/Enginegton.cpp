#include "pch.h"
#include "Enginegton.h"

Enginegton::Enginegton(const char* path, const char* priv_path) : terminate(false) {

	std::string p(path);
	std::string pp(priv_path);
	path_to_log = p;
	private_log_path = pp; 

	std::mt19937 mt;
	std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);

	// the duration of the following for loop corresponds to Chessington's loading screen time 

	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {

			CheckLog(); // to make it possible to quit Chessington during loading screen 
			if (request == "term") {
				terminate = true; 
				return;
			}

			int o = i * 8 + j;

			// generate all move and control masks/scan indices/magic bitboards 
			origin_masks[o] = (1ULL << o);
			if (i > 0 && i < 7) {
				white_pawn_control_bitboards[o] = PlayerPawnControl(i, j);
				white_pawn_push_bitboards[o] = PlayerPawnPush(i, j);
				black_pawn_control_bitboards[o] = OpponentPawnControl(i, j);
				black_pawn_push_bitboards[o] = OpponentPawnPush(i, j);
				w_pawn_scan_indices[o] = GetIndices(white_pawn_control_bitboards[o] | white_pawn_push_bitboards[o]);
				b_pawn_scan_indices[o] = GetIndices(black_pawn_control_bitboards[o] | black_pawn_push_bitboards[o]);
			}

			king_control_bitboards[o] = KingControl(i, j);
			king_scan_indices[o] = GetIndices(king_control_bitboards[o]);
			knight_control_bitboards[o] = KnightControl(i, j);
			knight_scan_indices[o] = GetIndices(knight_control_bitboards[o]);
			rook_control_bitboards[o] = RookFreeControl(i, j);
			rook_scan_indices[o] = GetIndices(rook_control_bitboards[o]);
			bishop_control_bitboards[o] = BishopFreeControl(i, j);
			bishop_scan_indices[o] = GetIndices(bishop_control_bitboards[o]);

			// king area is an extended area around the king // it can be used for new evaluation parameters in the future  
			king_areas[o] = king_control_bitboards[o] | KingArea(i, j) | knight_control_bitboards[o];

			GetMagics(o, true); // get magic numbers for attacks lookup of this square (o) for rooks 
			GetMagics(o, false); // for bishops 

			// generate long long int values for the zobrist hashing table 
			for (int k = 0; k < 12; ++k) zob_table[o][k] = dist(mt);
		}
	}

	// generate the remaining zobrist values 
	zob_turn = dist(mt);
	for (int i = 0; i < 16; ++i) zob_castle[i] = dist(mt);
	for (int i = 0; i < 8; ++i) zob_en_passant[i] = dist(mt);

	// this reservation can be made more dynamically in the future, by some approximation from the number of pieces, or initial moves, and max_depth
	transposition_table.reserve(5000000); 
	pinned_pieces.reserve(8); // 8 pieces can be pinned at one time (8 paths to the king for sliding pieces)
	hash_log.reserve(max_depth + 1);
	capture_log.reserve(max_depth * 2); 
	castle_log.reserve(max_depth * 2 + 2);
}


void Enginegton::Run() {

	Post("ok"); // notify Chessington that the constructor call has finished, so it can proceed to main menu 

	auto start = std::chrono::high_resolution_clock::now();

	while (!terminate) {

		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() > max_search_time && timeout == false) 
			timeout = true;

		request.clear();

		// peek the file, get Chessington's request, set "request" if it matches one of the 4 main request types 
		CheckLog();
		
		if (request.empty()) continue;

		if (request == "find") {
			stop_search = false; 
			timeout = false; 

			// "stop_search" and "timeout" are modified only in this thread, and read in SearchThread 
			// "searching" is modified only in SearchThread, and read in this 

			std::thread search_thread(&Enginegton::SearchThread, this);
			search_thread.detach();
			while (!searching) { ; } // wait until SearchThread sets "searching" to true
			start = std::chrono::high_resolution_clock::now();
			Post("ok"); 
		}
		else if (request == "get") {
			std::vector<std::string> r_out = GetReadOut();
			Post("ok"); 
			GetMoves(r_out);
		}
		else if (request == "stop") {
			if (stop_search) Post("ok"); // in case the engine is not working, post straight away 
			stop_search = true; // in case it is working, "ok" will be posted at the end of SearchThread() 
		}
		else {
			Post("ok");
			terminate = true; 
		}
	}
}

void Enginegton::CheckLog() {
	std::ifstream log(path_to_log);
	std::string line;
	std::getline(log, line);
	if (requests.count(line) > 0 && !line.empty()) request = line;
	log.close();
}

void Enginegton::SearchThread() {
	std::vector<std::string> r_out = GetReadOut();
	FindMove(r_out); // starts the recursive search 
	if (stop_search) Post("ok"); 
}

std::vector<std::string> Enginegton::GetReadOut() {
	std::vector<std::string> read_out;
	std::ifstream log(path_to_log);
	std::string line;
	while (std::getline(log, line)) read_out.push_back(line);
	log.close();
	return read_out;
}

void Enginegton::Post(std::string res) {
	std::ofstream log(path_to_log);
	log << res;
	log.close();
}

void Enginegton::WaitForEmptyLog() {
	while (true) {
		std::ifstream log(path_to_log);
		if (log.peek() == std::ifstream::traits_type::eof()) break;
		log.close();
	}
}

// flip the move if Chessington passed in a flipped board 
void Enginegton::Flip(Move& move) {
	move.origin = 63 - move.origin;
	move.destination = 63 - move.destination;
}

void Enginegton::PostMove(Move& move) {
	WaitForEmptyLog();
	if (orientation == -1) Flip(move);
	int or_f = move.origin % 8;
	int des_f = move.destination % 8;
	Post("f/" + std::to_string((move.origin - or_f) >> 3) + std::to_string(or_f)
		+ std::to_string((move.destination - des_f) >> 3) + std::to_string(des_f) + std::to_string(move.type));
}

void Enginegton::ParseRequest(std::string& b, std::string& t, std::string& m, std::string& c) {

	// GetReadOut() divides the data in the request to board (b), turn and orientation (t), last move (m), and castling rights (c) information 

	// the turn variable is used to denote piece colours - GetPieces(turn) return white_pieces& if turn == 1
	if (t[0] == '-') turn = -1;
	else turn = 1;

	std::vector<int> board;
	private_log_board.clear();
	hash_log.clear();
	board_state_bitboard = 0;
	unsigned long long int hash = 0;
	// build the board and board_state_bitboard 
	if (t[1] != '-') {
		orientation = 1;
		for (int i = 0; i < 64; ++i) {
			int to_int = fen_read[b[i]];
			if (to_int != -1) {
				board_state_bitboard |= (1ULL << i);
				hash ^= zob_table[i][to_int];
			}
			board.push_back(to_int);
			private_log_board += b[i];
			private_log_board += "  ";
			if ((i + 1) % 8 == 0) private_log_board += "\n";
		}
	}
	else {
		orientation = -1; // the engine always works from White's perspective and stores the request's orientation in case the output needs to be flipped back
		for (int i = 63; i >= 0; --i) {
			int to_int = fen_read[b[i]];
			if (to_int != -1) {
				board_state_bitboard |= (1ULL << (63 - i));
				hash ^= zob_table[i][to_int];
			}
			board.push_back(to_int);
			private_log_board += b[i];
			private_log_board += "  ";
			if ((64 - i) % 8 == 0) private_log_board += "\n";
		}
	}
	
	// get castling rights 
	int c_rights = std::stoi(c);
	int b_castle = c_rights % 4;
	int w_castle = (c_rights - b_castle) >> 2;

	// initialise piece groups 
	white_pieces = PieceGroup(board, 1, w_castle, 
		zob_table[56][3], zob_table[63][3], zob_table[61][3], zob_table[59][3]);
	black_pieces = PieceGroup(board, -1, b_castle, 
		zob_table[0][9], zob_table[7][9], zob_table[5][9], zob_table[3][9]);

	hash ^= zob_castle[(white_pieces.castling_rights << 2) + black_pieces.castling_rights];
	if (turn == -1) hash ^= zob_turn;

	en_passant_mask = 0;
	if (m[0] == '-') { // if last move was not a pawn push 
		hash_log.push_back(hash);
		return;
	}

	std::vector<int> move;
	if (orientation == 1) move = { m[0] - '0', m[1] - '0', m[2] - '0', m[3] - '0' };
	else move = { 7 - (m[0] - '0'), 7 - (m[1] - '0'), 7 - (m[2] - '0'), 7 - (m[3] - '0') };

	// if last move was a pawn push, and en passant is legal in the current position, the intiial hash position needs to be updated 
	if (move[1] == move[3] && abs(move[0] - move[2]) == 2 && (board[move[2] * 8 + move[3]] == 0 || board[move[2] * 8 + move[3]] == 6)) {
		if (turn == 1) en_passant_mask = origin_masks[move[0] * 8 + move[1] + 8];
		else en_passant_mask = origin_masks[move[0] * 8 + move[1] - 8];
		hash ^= zob_en_passant[move[1]];
	}
	hash_log.push_back(hash);
}

void Enginegton::PrivateLog(Move& move, unsigned int table_s, float eval, float nps) {

	std::string out =
		private_log_board + "\n"
		+ "Max depth positions reached: " + std::to_string(pos_count) + "\n"
		+ "Average search speed: " + std::to_string((int)nps) + " nodes/s\n"
		+ "Checkmates found: " + std::to_string(checkmate_count) + "\n" 
		+ "Transposition table size: " + std::to_string(table_s) + "\n"
		+ "Final evaluation: " + std::to_string(eval) + "\n\n" 
		+ OutUINT64(move.move_mask) + "\n";

	std::ofstream cl;
	cl.open(private_log_path, std::ofstream::out | std::ofstream::trunc);
	cl.close();

	std::ofstream log(private_log_path);
	log << out;
	log.close();
}


void Enginegton::GetMoves(std::vector<std::string>& r_out) {

	ParseRequest(r_out[1], r_out[2], r_out[3], r_out[4]); // set the engine's parameters 

	MoveQueue q(128);
	MovesGen(q); // get the legal moves for this position 

	// this block is responsible for catching insufficient material draw condition; checkmate cannot be forced if material is below 50 and no pawns are left to be promoted, or 
	// if material exceeds rook's value but there are two knights left 
	int material = 0; 
	bool white_mat = (white_pieces.map.size() == 3 && white_pieces.KnightCount() == 2) || (white_pieces.material < 50 && white_pieces.PawnCount() == 0); 
	bool black_mat = (black_pieces.map.size() == 3 && black_pieces.KnightCount() == 2) || (black_pieces.material < 50 && black_pieces.PawnCount() == 0);
	if (white_mat && black_mat) ++material;

	// *the engine does not keep track of threefold repetition, for now 

	// dequeue each legal move and convert to a string 
	std::string res = "/";
	while (q.GetSize() > 0) {
		Move move = q.Dequeue();
		if (orientation == -1) Flip(move);
		int or_f = move.origin % 8;
		int des_f = move.destination % 8;
		std::string m = std::to_string((move.origin - or_f) >> 3) + std::to_string(or_f)
			+ std::to_string((move.destination - des_f) >> 3) + std::to_string(des_f) + std::to_string(move.type);
		res += m;
	}
	res += "\n";
	std::string d = std::to_string(check > 0) + std::to_string(checkmate) + std::to_string(stalemate) + std::to_string(material);
	res += d;
	WaitForEmptyLog();
	Post(res);
}

void Enginegton::FindMove(std::vector<std::string>& r_out) {

	early_stop = false; 
	pos_count = 0; total_node_count = 0; checkmate_count = 0; 
	ParseRequest(r_out[1], r_out[2], r_out[3], r_out[4]);

	transposition_table.clear();
	capture_log.clear();
	castle_log.clear();

	castle_log.push_back(white_pieces.castling_rights);
	castle_log.push_back(black_pieces.castling_rights);

	// define evaluation parameters that are dependent on the data in the request 
	initial_phase = GetInitialGamePhase();
	if (white_pieces.map.size() > black_pieces.map.size()) max_pieces_eval = 1.0f / ((float)white_pieces.map.size() * 100.0f);
	else max_pieces_eval = 1.0f / ((float)black_pieces.map.size() * 100.0f); // the side with the most pieces defines the max_pieces_eval multiplier 

	// in a typical rook-on-king checkmating pattern, and its variations, the engine increases depth to avoid the embarassment of obtaining a winning position and failing to convert 
	if (white_pieces.material + black_pieces.material <= 90 || (white_pieces.material == 0 || black_pieces.material == 0)) max_depth = 8;
	else max_depth = 6; 

	searching = true; 
	auto start = std::chrono::high_resolution_clock::now();

	Move move; 
	MoveQueue q = MoveQueue(128);
	MovesGen(q);

	float e = 0.0f; 

	if (q.GetSize() == 1) {
		move = q.Dequeue();
		WaitForEmptyLog();
		PostMove(move);
		searching = false;
		return;
	}

	if (turn == 1) {

		float max_eval = std::numeric_limits<float>::lowest();
		float alpha = max_eval; 

		while (q.GetSize() > 0) {

			if (early_stop || stop_search || terminate || timeout) break;

			Move next = q.Dequeue();
			float eval = Calculate(1, next, alpha, std::numeric_limits<float>::infinity());
			UndoMove(next);

			if (eval > alpha) alpha = eval;
			if (eval > max_eval) {
				max_eval = eval;
				move = next;
			}
		}
		e = max_eval; 
	}
	else {

		float min_eval = std::numeric_limits<float>::infinity();
		float beta = min_eval;

		while (q.GetSize() > 0) {

			if (early_stop || stop_search || terminate || timeout) break;

			Move next = q.Dequeue();
			float eval = Calculate(1, next, std::numeric_limits<float>::lowest(), beta);
			UndoMove(next);

			if (eval < beta) beta = eval;
			if (eval < min_eval) {
				min_eval = eval;
				move = next;
			}
		}
		e = min_eval;
	}

	unsigned int table_s = transposition_table.size();
	transposition_table.clear(); // clear the tranposition table now because it is not known when the next call will take place 
	// (and the table routinely fills up with hundreds of MB)

	searching = false;

	if (!stop_search && !terminate) {
		float secs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.f;
		PrivateLog(move, table_s, e, total_node_count / secs);
		WaitForEmptyLog();
		PostMove(move);
	}
}


float Enginegton::Calculate(int depth, const Move& move, float alpha, float beta) {

	if (depth < max_depth) {

		++total_node_count;
		ProcessMove(move);
		unsigned long long int current_hash = hash_log.back();

		// with a low search depth, the following transposition table check can hinder decision-making 
		// for example, if the engine reasons that some repetition of moves between depths 1-4 is optimal, the static evaluation at depth 6 will be saved, but its real depth is 2. 
		// if this position is then reached after 2 moves, starting from the initial position, the engine will return this value below. If what follows significantly changed
		// the picture of the game, the engine would miss it.
		// This could be mitigated by accepting the transposition_table value at higher depths only
		// 
		//if (transposition_table.count(current_hash) > 0) return transposition_table[current_hash];

		MoveQueue q = MoveQueue(128);
		MovesGen(q);

		if (stalemate) {
			transposition_table[current_hash] = 0.0f;
			return 0.0f;
		}

		if (turn == 1) {
			if (checkmate) {
				if (depth == 1) early_stop = true;
				transposition_table[current_hash] = -10000.0f + depth;
				return -10000.0f + depth;
			}

			float max_eval = std::numeric_limits<float>::lowest();
			while (q.GetSize() > 0) {

				if (stop_search || terminate || timeout) break;

				Move next = std::move(q.Dequeue());
				float eval = Calculate(depth + 1, next, alpha, beta);
				UndoMove(next);

				if (eval > alpha) alpha = eval;
				if (eval > max_eval) max_eval = eval;
				if (beta <= alpha) break;
				
			}
			return max_eval; 
		}
		else {
			if (checkmate) {
				if (depth == 1) early_stop = true; 
				transposition_table[current_hash] = 10000.0f - depth;
				return 10000.0f - depth;
			}

			float min_eval = std::numeric_limits<float>::infinity();
			while (q.GetSize() > 0) {

				if (stop_search || terminate || timeout) break;

				Move next = std::move(q.Dequeue());
				float eval = Calculate(depth + 1, next, alpha, beta);
				UndoMove(next);

				if (eval < beta) beta = eval;
				if (eval < min_eval) min_eval = eval;
				if (beta <= alpha) break;
			}
			return min_eval;
		}
	}

	else { // maximum depth 

		ProcessMove(move);
		++pos_count;
		++total_node_count;

		unsigned long long int current_hash = hash_log.back();
		if (transposition_table.count(current_hash) > 0) return transposition_table[current_hash];

		int phase = GetGamePhase();
		WaitingSide(phase);
		MovingSide();

		if (stalemate) {
			transposition_table[current_hash] = 0.0f;
			return 0.0f;
		}

		if (checkmate) {
			++checkmate_count;
			if (turn == 1) {
				transposition_table[current_hash] = -10000.0f + depth, board_state_bitboard;
				return -10000.0f + depth;
			}
			else {
				transposition_table[current_hash] = 10000.0f - depth;
				return 10000.0f - depth;
			}
		}

		EvaluateMovingSide(phase);

		float eval = Evaluate(phase);
		transposition_table[current_hash] = eval;
		return eval;
	}
}

void Enginegton::ProcessMove(const Move& move) {

	unsigned long long int new_hash = hash_log.back();
	int piece = GetPieces(turn).GetPieceID(move.origin);
	new_hash ^= zob_table[move.origin][piece];
	new_hash ^= zob_table[move.destination][piece];

	int rem_piece = GetPieces(-turn).GetPieceID(move.destination);
	int rem_sqr = 0;
	if (rem_piece > -1) {
		rem_sqr = move.destination;
		new_hash ^= zob_table[rem_sqr][rem_piece];
		GetPieces(-turn).RemovePiece(rem_sqr, origin_masks[rem_sqr]);
	}

	if (move.type != 0) {

		switch (move.type) {

			// promotion 
		case 1:
		case 2:
		case 3:
		case 4:
			GetPieces(turn).Promote(move.origin, Promote(move.type));
			new_hash ^= zob_table[move.destination][Unpromote()];
			new_hash ^= zob_table[move.destination][Promote(move.type)];
			break;
		case 5: // castle kingside 
			GetPieces(turn).CastleShort(new_hash);
			break;
		case 6: // castle queenside 
			GetPieces(turn).CastleLong(new_hash);
			break;
		case 7: // white captures en passant
			rem_sqr = move.destination + 8;
			rem_piece = 6;
			GetPieces(-turn).RemovePiece(rem_sqr, origin_masks[rem_sqr]);
			new_hash ^= zob_table[rem_sqr][rem_piece];
			break;
		case 8: // black captures en passant
			rem_sqr = move.destination - 8;
			rem_piece = 0;
			GetPieces(-turn).RemovePiece(rem_sqr, origin_masks[rem_sqr]);
			new_hash ^= zob_table[rem_sqr][rem_piece];
			break;
		}
	}

	// log the capture - if none, rem_piece == -1 
	capture_log.push_back(rem_sqr);
	capture_log.push_back(rem_piece);

	GetPieces(turn).UpdatePiecePosition(move.origin, move.destination, origin_masks[move.origin], origin_masks[move.destination]);

	if ((piece == 0 || piece == 6) && abs(move.origin - move.destination) == 16) {
		int en_p_sqr = move.destination + 8 * turn;
		en_passant_mask = origin_masks[en_p_sqr];
		if ((GetPieces(-turn).GetPieceID(move.destination - 1) == 0 && (en_passant_mask & white_pawn_control_bitboards[move.destination - 1]) > 0)
			|| (GetPieces(-turn).GetPieceID(move.destination - 1) == 6 && (en_passant_mask & black_pawn_control_bitboards[move.destination - 1]) > 0)
			 || (GetPieces(-turn).GetPieceID(move.destination + 1) == 0 && (en_passant_mask & white_pawn_control_bitboards[move.destination + 1]) > 0)
			|| (GetPieces(-turn).GetPieceID(move.destination + 1) == 6 && (en_passant_mask & black_pawn_control_bitboards[move.destination + 1]) > 0)) {
			new_hash ^= zob_en_passant[en_p_sqr % 7];
		}
	}
	else en_passant_mask = 0;

	new_hash ^= zob_castle[(white_pieces.castling_rights << 2) + black_pieces.castling_rights];

	// log updated castling rights 
	castle_log.push_back(white_pieces.castling_rights);
	castle_log.push_back(black_pieces.castling_rights);

	board_state_bitboard = white_pieces.group_bitboard | black_pieces.group_bitboard;
	turn = -turn;
	if (turn == -1) new_hash ^= zob_turn; 
	hash_log.push_back(new_hash);
}

void Enginegton::UndoMove(const Move& move) {

	turn = -turn;

	hash_log.pop_back();

	if (move.type != 0) {
		switch (move.type) {

		case 1:
		case 2:
		case 3:
		case 4:
			GetPieces(turn).Promote(move.destination, Unpromote());
			break;
		case 5:
			GetPieces(turn).UndoCastleShort();
			break;
		case 6:
			GetPieces(turn).UndoCastleLong();
			break;
		}
	}

	GetPieces(turn).UpdatePiecePosition(move.destination, move.origin, origin_masks[move.destination], origin_masks[move.origin]);

	int add_back = capture_log.back();
	capture_log.pop_back();
	int add_sqr = capture_log.back();
	capture_log.pop_back();

	if (add_back > -1) GetPieces(-turn).AddPiece(add_sqr, add_back, origin_masks[add_sqr]);

	castle_log.pop_back();
	castle_log.pop_back();
	black_pieces.castling_rights = castle_log.back();
	white_pieces.castling_rights = castle_log[castle_log.size() - 2];
	
	board_state_bitboard = white_pieces.group_bitboard | black_pieces.group_bitboard;
}

int Enginegton::Promote(int in) {
	if (turn == -1) return (in + 6);
	else return in; 
}

int Enginegton::Unpromote() {
	if (turn == -1) return 6;
	else return 0;
}


// Magics generation 

void Enginegton::GetMagics(int origin, bool rook) {

	uint64_t mask = rook ? RookMask(origin) : BishopMask(origin);
	if (rook) rook_masks[origin] = mask;
	else bishop_masks[origin] = mask;

	std::vector<int> indices = GetIndices(mask);
	int num = 1 << indices.size();

	std::vector<uint64_t> all_blocks;
	all_blocks.reserve(num);

	for (int i = 0; i < num; ++i) {
		uint64_t blockers = 0;
		for (int j = 0; j < indices.size(); ++j) {
			uint64_t bit = (i >> j) & 1;
			blockers |= (bit << indices[j]);
		}
		all_blocks.push_back(blockers);
	}

	std::mt19937 mt;
	std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);

	bool found = false;
	while (!found) {

		unsigned long long int magic = dist(mt) & dist(mt) & dist(mt);
		bool taken[4096] = {};
		
		int max = rook ? 4095 : 511;
		for (int i = 0; i < num; ++i) {

			int key = rook ? (all_blocks[i] * magic) >> (64 - rook_index_shifts[origin]) : (all_blocks[i] * magic) >> (64 - bishop_index_shifts[origin]);
			if (key > max || key < 0) break;
			if (taken[key]) break;

			if (rook) rook_magic_bitboards[origin][key] = RookAttack(origin, all_blocks[i]);
			else bishop_magic_bitboards[origin][key] = BishopAttack(origin, all_blocks[i]);

			taken[key] = true;
			if (i == num - 1) {
				if (rook) rook_magics[origin] = magic;
				else bishop_magics[origin] = magic;
				found = true;
			}
		}
	}
}

// Move gen helpers for rooks and bishops 

uint64_t Enginegton::RookControl(int origin, uint64_t king_mask, int king_sqr) {
	// the magic number brings the blocker bits to the most significant 12 bits of the uint, and the >> shift brings these bits to the least significant 12 bits, 
	// e.g., for a rook, giving a key in the range up to 4096 (2 ** 12) entries, for the maximum number of blocker configurations (without the edge squares)
	uint64_t blockers = (board_state_bitboard & rook_masks[origin]) & ~king_mask; 
	// we take the king out so that the piece x-rays the king and makes the squares behind unavailable 

	// a possible optimisation, to avoid getting the other attack in case of a check, would be to just add that one square behind the king depending on the positioning of the rook

	int k = (blockers * rook_magics[origin]) >> (64 - rook_index_shifts[origin]);
	uint64_t control = rook_magic_bitboards[origin][k];

	if ((control & king_mask) > 0) {
		++check;
		uint64_t blockers_with_king = (board_state_bitboard & rook_masks[origin]); 
		int n_k = (blockers_with_king * rook_magics[origin]) >> (64 - rook_index_shifts[origin]);
		int k_k = ((board_state_bitboard & rook_masks[king_sqr]) * rook_magics[king_sqr]) >> (64 - rook_index_shifts[king_sqr]);
		check_map = (rook_magic_bitboards[origin][n_k] & rook_magic_bitboards[king_sqr][k_k]) | origin_masks[origin];
		return control;
	}

	if ((rook_control_bitboards[origin] & king_mask) > 0) {

		int k_k = ((board_state_bitboard & rook_masks[king_sqr]) * rook_magics[king_sqr]) >> (64 - rook_index_shifts[king_sqr]);
		uint64_t intersection = rook_magic_bitboards[king_sqr][k_k] & control;
		if (intersection == 0) return control; 

		int sqr = (origin < king_sqr) ? RayIndex(intersection, origin) : RayIndex(intersection, king_sqr);
		if (GetPieces(turn).GetPieceID(sqr) != -1) {
			pinned_pieces[sqr] = (rook_control_bitboards[king_sqr] & rook_control_bitboards[origin]) | origin_masks[origin];
		}
	}

	return control;
}

uint64_t Enginegton::BishopControl(int origin, uint64_t king_mask, int king_sqr) {

	uint64_t blockers = (board_state_bitboard & bishop_masks[origin]) & ~king_mask; 
	int k = (blockers * bishop_magics[origin]) >> (64 - bishop_index_shifts[origin]);
	uint64_t control = bishop_magic_bitboards[origin][k];

	if ((control & king_mask) > 0) {
		++check;
		uint64_t blockers_with_king = (board_state_bitboard & bishop_masks[origin]);
		int n_k = (blockers_with_king * bishop_magics[origin]) >> (64 - bishop_index_shifts[origin]);
		int k_k = ((board_state_bitboard & bishop_masks[king_sqr]) * bishop_magics[king_sqr]) >> (64 - bishop_index_shifts[king_sqr]);
		check_map = (bishop_magic_bitboards[origin][n_k] & bishop_magic_bitboards[king_sqr][k_k]) | origin_masks[origin];
		return control; 
	}	

	if ((bishop_control_bitboards[origin] & king_mask) > 0) {
		
		int k_k = ((board_state_bitboard & bishop_masks[king_sqr]) * bishop_magics[king_sqr]) >> (64 - bishop_index_shifts[king_sqr]);
		uint64_t intersection = bishop_magic_bitboards[king_sqr][k_k] & control;
		if (intersection == 0) return control;

		int sqr = (origin < king_sqr) ? RayIndex(intersection, origin) : RayIndex(intersection, king_sqr);
		if (GetPieces(turn).GetPieceID(sqr) != -1) {
			pinned_pieces[sqr] = (bishop_control_bitboards[king_sqr] & bishop_control_bitboards[origin]) | origin_masks[origin];
		}
	}
	return control;
}

uint64_t Enginegton::RookControl(int origin) {
	uint64_t blockers = board_state_bitboard & rook_masks[origin];
	int k = (blockers * rook_magics[origin]) >> (64 - rook_index_shifts[origin]);
	return rook_magic_bitboards[origin][k];
}

uint64_t Enginegton::BishopControl(int origin) {
	uint64_t blockers = board_state_bitboard & bishop_masks[origin];
	int k = (blockers * bishop_magics[origin]) >> (64 - bishop_index_shifts[origin]);
	return bishop_magic_bitboards[origin][k];
}

int Enginegton::RayIndex(uint64_t mask, int start) {
	while ((mask & (1ULL << start)) == 0) ++start;
	return start; 
}

uint64_t Enginegton::RookMoves(int origin, uint64_t own_pieces) {
	int k = ((board_state_bitboard & rook_masks[origin]) * rook_magics[origin]) >> (64 - rook_index_shifts[origin]);
	return rook_magic_bitboards[origin][k] & ~own_pieces;
}

uint64_t Enginegton::BishopMoves(int origin, uint64_t own_pieces) {
	int k = ((board_state_bitboard & bishop_masks[origin]) * bishop_magics[origin]) >> (64 - bishop_index_shifts[origin]);
	return bishop_magic_bitboards[origin][k] & ~own_pieces;
}

// Scans 

void Enginegton::WhitePawnBitScan(int origin, uint64_t moves, MoveQueue& queue) {

	int type = 0;

	for (int pos : w_pawn_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue; 
		if (pos >= 8) {
			if (move == en_passant_mask) type = 7;
			else type = 0;
			queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RatePawnMove(pos), type));
		}
		else {
			for (int i = 1; i < 5; ++i) queue.Enqueue(Move(move | origin_masks[origin], origin, pos, i + 1000, i));
		}

	}
}

void Enginegton::BlackPawnBitScan(int origin, uint64_t moves, MoveQueue& queue) {

	int type = 0;

	for (int pos : b_pawn_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		if (pos <= 55) {
			if (move == en_passant_mask) type = 8;
			else type = 0;
			queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RatePawnMove(pos), type));
		}
		else {
			for (int i = 1; i < 5; ++i) queue.Enqueue(Move(move | origin_masks[origin], origin, pos, i + 1000, i));
		}
	}
}

void Enginegton::KnightBitScan(int origin, uint64_t moves, MoveQueue& queue) {

	for (int pos : knight_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue; 
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateKnightMove(pos)));
	}
}

void Enginegton::WhiteKingBitScan(int origin, uint64_t moves, MoveQueue& queue, int castle, uint64_t block) {

	// Adding castle moves first, as they are likely to be among the highest rated moves, hence reducing the internal queue operations 
	if (castle == 1 || castle == 3) {
		if ((white_long_castle_bitboard & block) == 0 && (white_long_sub_bitboard & control) == 0)
			queue.Enqueue(Move(origin_masks[origin - 2] | origin_masks[origin], origin, origin - 2, 70, 6));
	}
	if (castle >= 2) {
		if ((white_short_castle_bitboard & block) == 0 && (white_short_castle_bitboard & control) == 0)
			queue.Enqueue(Move(origin_masks[origin + 2] | origin_masks[origin], origin, origin + 2, 100, 5));
	}

	for (int pos : king_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue; 
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateKingMove(pos)));
	}
}

void Enginegton::BlackKingBitScan(int origin, uint64_t moves, MoveQueue& queue, int castle, uint64_t block) {

	if (castle == 1 || castle == 3) {
		if ((black_long_castle_bitboard & block) == 0 && (black_long_sub_bitboard & control) == 0)
			queue.Enqueue(Move(origin_masks[origin - 2] | origin_masks[origin], origin, origin - 2, 70, 6));
	}
	if (castle >= 2) {
		if ((black_short_castle_bitboard & block) == 0 && (black_short_castle_bitboard & control) == 0)
			queue.Enqueue(Move(origin_masks[origin + 2] | origin_masks[origin], origin, origin + 2, 100, 5));
	} 

	for (int pos : king_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateKingMove(pos)));
	}
}

void Enginegton::BishopBitScan(int origin, uint64_t moves, MoveQueue& queue) {

	for (int pos : bishop_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateBishopMove(pos)));
	}
}

void Enginegton::RookBitScan(int origin, uint64_t moves, MoveQueue& queue) {
	
	for (int pos : rook_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateRookMove(pos)));
	}

}

void Enginegton::QueenBitScan(int origin, uint64_t moves, MoveQueue& queue) {

	for (int pos : rook_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateQueenMove(pos)));
	}
	
	for (int pos : bishop_scan_indices[origin]) {
		uint64_t move = moves & origin_masks[pos];
		if (move == 0) continue;
		queue.Enqueue(Move(move | origin_masks[origin], origin, pos, RateQueenMove(pos)));
	}

}


int Enginegton::BitCount(uint64_t bits, const std::vector<int>& indices) {
	int count = 0;
	for (int pos : indices) if ((bits & origin_masks[pos]) > 0) ++count; 
	return count;
}

int Enginegton::BitCount(uint64_t bits, int from) {
	int count = 0; 
	while (bits > 0) {
		uint64_t move = bits & origin_masks[from];
		if (move > 0) {
			bits ^= move;
			++count; 
		}
		--from;
	}
	return count; 
}


// Move generation 

void Enginegton::MovesGen(MoveQueue& queue, bool initial) {

	control = 0; check_map = 0; pinned_pieces.clear(); check = 0;
	checkmate = false; stalemate = false; 
	uint64_t king_mask = GetPieces(turn).king_mask;
	int king_sqr = GetPieces(turn).king_sqr;

	for (const auto& [origin, id] : GetPieces(-turn).map) {

		switch (id) {

		case 6:
			control |= black_pawn_control_bitboards[origin];
			if ((king_mask & black_pawn_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			break;

		case 0:
			control |= white_pawn_control_bitboards[origin];
			if ((king_mask & white_pawn_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			break;

		case 11:
		case 5:
			control |= king_control_bitboards[origin];
			break;

		case 7:
		case 1:
			control |= knight_control_bitboards[origin];
			if ((king_mask & knight_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			break;

		case 8:
		case 2:
			control |= BishopControl(origin, king_mask, king_sqr);
			break;

		case 9:
		case 3:
			control |= RookControl(origin, king_mask, king_sqr);
			break;

		case 10:
		case 4:
			control |= RookControl(origin, king_mask, king_sqr);
			control |= BishopControl(origin, king_mask, king_sqr);
			break;
		}
	}

	//////

	uint64_t blockers_mask = GetPieces(turn).group_bitboard;
	uint64_t opp_pieces_mask = GetPieces(-turn).group_bitboard | en_passant_mask;
	uint64_t pawn_moves = 0;

	if (check == 0) {

		for (const auto& [origin, id] : GetPieces(turn).map) {

			bool pinned = pinned_pieces.count(origin) > 0;

			switch (id) {

			case 6:
				if (pinned) {
					pawn_moves = (black_pawn_control_bitboards[origin] & opp_pieces_mask) & pinned_pieces[origin];
					if ((origin_masks[origin + 8] & board_state_bitboard) == 0) {
						pawn_moves |= (black_pawn_push_bitboards[origin] & ~board_state_bitboard) & pinned_pieces[origin];
					}
					BlackPawnBitScan(origin, pawn_moves, queue);
				}
				else {
					pawn_moves = black_pawn_control_bitboards[origin] & opp_pieces_mask;
					if ((origin_masks[origin + 8] & board_state_bitboard) == 0) {
						pawn_moves |= black_pawn_push_bitboards[origin] & ~board_state_bitboard;
					}
					BlackPawnBitScan(origin, pawn_moves, queue);
				}
				break;

			case 0:
				if (pinned) {
					pawn_moves = (white_pawn_control_bitboards[origin] & opp_pieces_mask) & pinned_pieces[origin];
					if ((origin_masks[origin - 8] & board_state_bitboard) == 0) {
						pawn_moves |= (white_pawn_push_bitboards[origin] & ~board_state_bitboard) & pinned_pieces[origin];
					}
					WhitePawnBitScan(origin, pawn_moves, queue);
				}
				else {
					pawn_moves = white_pawn_control_bitboards[origin] & opp_pieces_mask;
					if ((origin_masks[origin - 8] & board_state_bitboard) == 0) {
						pawn_moves |= white_pawn_push_bitboards[origin] & ~board_state_bitboard;
					}
					WhitePawnBitScan(origin, pawn_moves, queue);
				}
				break;

			case 11:
				BlackKingBitScan(origin, (king_control_bitboards[origin] & ~blockers_mask) & ~control, queue, GetPieces(turn).castling_rights, blockers_mask);
				break;
			case 5:
				WhiteKingBitScan(origin, (king_control_bitboards[origin] & ~blockers_mask) & ~control, queue, GetPieces(turn).castling_rights, blockers_mask);
				break;

			case 7:
			case 1:
				if (pinned) continue;
				KnightBitScan(origin, knight_control_bitboards[origin] & ~blockers_mask, queue);
				break;

			case 8:
			case 2:
				if (pinned) BishopBitScan(origin, BishopMoves(origin, blockers_mask) & pinned_pieces[origin], queue);
				else BishopBitScan(origin, BishopMoves(origin, blockers_mask), queue);
				break;

			case 9:
			case 3:
				if (pinned) RookBitScan(origin, RookMoves(origin, blockers_mask) & pinned_pieces[origin], queue);
				else RookBitScan(origin, RookMoves(origin, blockers_mask), queue);
				break;

			case 10:
			case 4:
				if (pinned) QueenBitScan(origin, (RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask)) & pinned_pieces[origin], queue);
				else QueenBitScan(origin, RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask), queue);
				break;
			}
		}
		if (queue.GetSize() == 0) stalemate = true;
	}

	else if (check == 1) {

		if (en_passant_mask != 0) {
			if ((en_passant_mask == (check_map << 8) && turn == -1) || (en_passant_mask == (check_map >> 8) && turn == 1)) {
				opp_pieces_mask |= check_map;
			}
			else opp_pieces_mask &= check_map;
		}
		else opp_pieces_mask &= check_map;

		for (const auto& [origin, id] : GetPieces(turn).map) {
			
			if (pinned_pieces.count(origin) > 0) continue;

			switch (id) {

			case 6:
				pawn_moves = black_pawn_control_bitboards[origin] & opp_pieces_mask;
				if ((origin_masks[origin + 8] & board_state_bitboard) == 0) {
					pawn_moves |= ((black_pawn_push_bitboards[origin] & ~board_state_bitboard) & check_map);
				}
				BlackPawnBitScan(origin, pawn_moves, queue);
				break;

			case 0:
				pawn_moves = white_pawn_control_bitboards[origin] & opp_pieces_mask;
				if ((origin_masks[origin - 8] & board_state_bitboard) == 0) {
					pawn_moves |= ((white_pawn_push_bitboards[origin] & ~board_state_bitboard) & check_map);
				}
				WhitePawnBitScan(origin, pawn_moves, queue);
				break;

			case 11:
			case 5:
				WhiteKingBitScan(origin, (king_control_bitboards[origin] & ~blockers_mask) & ~control, queue);
				break;

			case 7:
			case 1:
				KnightBitScan(origin, knight_control_bitboards[origin] & check_map, queue);
				break;

			case 8:
			case 2:
				BishopBitScan(origin, BishopMoves(origin, blockers_mask) & check_map, queue);
				break;

			case 9:
			case 3:
				RookBitScan(origin, RookMoves(origin, blockers_mask) & check_map, queue);
				break;

			case 10:
			case 4:
				QueenBitScan(origin, (RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask)) & check_map, queue);
				break;
			}
		}

		if (queue.GetSize() == 0) checkmate = true;
	}

	else {
		WhiteKingBitScan(GetPieces(turn).king_sqr, (king_control_bitboards[GetPieces(turn).king_sqr] & ~blockers_mask) & ~control, queue);
		if (queue.GetSize() == 0) checkmate = true;
	}
}

void Enginegton::WaitingSide(int phase) {

	control = 0;
	check_map = 0;
	pinned_pieces.clear();
	check = 0;
	checkmate = false; stalemate = false;
	uint64_t king_mask = GetPieces(turn).king_mask;
	int king_sqr = GetPieces(turn).king_sqr;

	white_pieces_eval = 0;
	black_pieces_eval = 0;

	int b_count = 0; 

	for (const auto& [origin, id] : GetPieces(-turn).map) {

		switch (id) {

		case 6:

			control |= black_pawn_control_bitboards[origin];
			if ((king_mask & black_pawn_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			black_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 0:
			white_pieces_eval += eval_tables[phase][id][origin];
			control |= white_pawn_control_bitboards[origin];
			if ((king_mask & white_pawn_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			break;

		case 11:
			control |= king_control_bitboards[origin];
			black_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 5:
			white_pieces_eval += eval_tables[phase][id][origin];
			control |= king_control_bitboards[origin];
			break;

		case 7:
			control |= knight_control_bitboards[origin];
			if ((king_mask & knight_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			black_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 1:
			white_pieces_eval += eval_tables[phase][id][origin];
			control |= knight_control_bitboards[origin];
			if ((king_mask & knight_control_bitboards[origin]) > 0) {
				++check;
				check_map = 1ULL << origin;
			}
			break;

		case 8:
			control |= BishopControl(origin, king_mask, king_sqr);
			black_pieces_eval += eval_tables[phase][id][origin];
			++b_count; 
			break;

		case 2:
			++b_count; 
			control |= BishopControl(origin, king_mask, king_sqr);
			white_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 9:
			control |= RookControl(origin, king_mask, king_sqr);
			black_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 3:
			control |= RookControl(origin, king_mask, king_sqr);
			white_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 10:
			control |= RookControl(origin, king_mask, king_sqr);
			control |= BishopControl(origin, king_mask, king_sqr);
			black_pieces_eval += eval_tables[phase][id][origin];
			break;

		case 4:
			control |= RookControl(origin, king_mask, king_sqr);
			control |= BishopControl(origin, king_mask, king_sqr);
			white_pieces_eval += eval_tables[phase][id][origin];
			break;
		}
	}

	if (b_count > 1) {
		if (turn == 1) black_pieces_eval += 200;
		else white_pieces_eval += 200; 
	}
}

void Enginegton::MovingSide() {

	uint64_t blockers_mask = GetPieces(turn).group_bitboard;
	uint64_t opp_pieces_mask = GetPieces(-turn).group_bitboard | en_passant_mask;
	uint64_t pawn_moves = 0;

	if (check == 0) {

		for (const auto& [origin, id] : GetPieces(turn).map) {

			bool pinned = pinned_pieces.count(origin) > 0;

			switch (id) {

			case 6:

				if (pinned) {
					pawn_moves |= (black_pawn_control_bitboards[origin] & opp_pieces_mask) & pinned_pieces[origin];
					if ((origin_masks[origin + 8] & board_state_bitboard) == 0) {
						pawn_moves |= (black_pawn_push_bitboards[origin] & ~board_state_bitboard) & pinned_pieces[origin];
					}
				}
				else {
					pawn_moves |= black_pawn_control_bitboards[origin] & opp_pieces_mask;
					if ((origin_masks[origin + 8] & board_state_bitboard) == 0) {
						pawn_moves |= black_pawn_push_bitboards[origin] & ~board_state_bitboard;
					}
				}
				if (pawn_moves > 0) return;
				break;

			case 0:

				if (pinned) {
					pawn_moves |= (white_pawn_control_bitboards[origin] & opp_pieces_mask) & pinned_pieces[origin];
					if ((origin_masks[origin - 8] & board_state_bitboard) == 0) {
						pawn_moves |= (white_pawn_push_bitboards[origin] & ~board_state_bitboard) & pinned_pieces[origin];
					}
				}
				else {
					pawn_moves |= white_pawn_control_bitboards[origin] & opp_pieces_mask;
					if ((origin_masks[origin - 8] & board_state_bitboard) == 0) {
						pawn_moves |= white_pawn_push_bitboards[origin] & ~board_state_bitboard;
					}
				}
				if (pawn_moves > 0) return;
				break;

			case 11:
			case 5:
				if (((king_control_bitboards[origin] & ~blockers_mask) & ~control) > 0) return; // if the king has no ordinary moves, then it cannot castle either
				break;

			case 7:
			case 1:
				if (pinned) continue;
				if ((knight_control_bitboards[origin] & ~blockers_mask) > 0) return;
				break;

			case 8:
			case 2:
				if (pinned) {
					if ((BishopMoves(origin, blockers_mask) & pinned_pieces[origin]) > 0) return;
				}
				else {
					if (BishopMoves(origin, blockers_mask) > 0) return;
				}
				break;

			case 9:
			case 3:
				if (pinned) {
					if ((RookMoves(origin, blockers_mask) & pinned_pieces[origin]) > 0) return;
				}
				else {
					if (RookMoves(origin, blockers_mask) > 0) return;
				}
				break;

			case 10:
			case 4:
				if (pinned) {
					if (((RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask)) & pinned_pieces[origin]) > 0) return;
				}
				else {
					if ((RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask)) > 0) return; 
				}
				break;
			}
		}
	}

	else if (check == 1) {

		if (en_passant_mask != 0) {
			if ((en_passant_mask == (check_map << 8) && turn == -1) || (en_passant_mask == (check_map >> 8) && turn == 1)) {
				opp_pieces_mask |= check_map;
			}
			else opp_pieces_mask &= check_map;
		}
		else opp_pieces_mask &= check_map;

		for (const auto& [origin, id] : GetPieces(turn).map) {

			bool pinned = pinned_pieces.count(origin) > 0;

			switch (id) {

			case 6:
				if (pinned) continue;
				pawn_moves |= black_pawn_control_bitboards[origin] & opp_pieces_mask;
				if ((origin_masks[origin + 8] & board_state_bitboard) == 0) pawn_moves |= ((black_pawn_push_bitboards[origin] & ~board_state_bitboard) & check_map);
				if (pawn_moves > 0) return;
				break;

			case 0:
				if (pinned) continue;
				pawn_moves |= white_pawn_control_bitboards[origin] & opp_pieces_mask;
				if ((origin_masks[origin - 8] & board_state_bitboard) == 0) pawn_moves |= ((white_pawn_push_bitboards[origin] & ~board_state_bitboard) & check_map);
				if (pawn_moves > 0) return; 
				break;

			case 11:
			case 5:
				if (((king_control_bitboards[origin] & ~blockers_mask) & ~control) > 0) return; 
				break;

			case 7:
			case 1:
				if (pinned) continue;
				if ((knight_control_bitboards[origin] & check_map) > 0) return; 
				break;

			case 8:
			case 2:
				if (pinned) continue;
				if ((BishopMoves(origin, blockers_mask) & check_map) > 0) return; 
				break;

			case 9:
			case 3:
				if (pinned) continue;
				if ((RookMoves(origin, blockers_mask) & check_map) > 0) return;
				break;

			case 10:
			case 4:
				if (pinned) continue;
				if (((RookMoves(origin, blockers_mask) | BishopMoves(origin, blockers_mask)) & check_map) > 0) return; 
				break;

			}
		}
	}

	else {
		if (((king_control_bitboards[GetPieces(turn).king_sqr] & ~blockers_mask) & ~control) > 0) return;
	}

	// if the function did not return earlier, it means that no moves were found and the game is over 
	if (check > 0) checkmate = true;
	else stalemate = true; 
}



// Evaluation 

int Enginegton::GetDevelopment(int col) {
	int dev = 0; 
	int ind = col == 1 ? 0 : 6;
	for (int i = 0; i < 8; ++i) {
		if (GetPieces(col).GetPieceID(i) != backrank[i] + ind) ++dev;
	}
	if (col == 1) white_development = dev * max_dev;
	else black_development = dev * max_dev;
	return dev; 
}

int Enginegton::GetGamePhase() {

	int total_piece_material = white_pieces.piece_material + black_pieces.piece_material; 
	if (total_piece_material < 210 || initial_phase == 2) return 2; 

	if (GetDevelopment(1) + GetDevelopment(-1) >= 12 || initial_phase > 0) return 1;

	return 0; 

}

int Enginegton::GetInitialGamePhase() {

	// For now almost the same as GetGamePhase, but it will be more detailed in the future and work more closely with initial move ordering 

	int total_piece_material = white_pieces.piece_material + black_pieces.piece_material;
	if (total_piece_material < 21) return 2;

	if (GetDevelopment(1) + GetDevelopment(-1) >= 12) return 1;

	return 0;

}

void Enginegton::EvaluateMovingSide(int phase) {

	int eval = 0;
	int b_count = 0; 
	for (const auto& [origin, id] : GetPieces(turn).map) {
		eval += eval_tables[phase][id][origin];
		if (id == 2 || id == 8) ++b_count; 
	}
	if (b_count > 1) eval += 200; 
	if (turn == 1) white_pieces_eval = eval;
	else black_pieces_eval = eval;
}

float Enginegton::Evaluate(int phase) {

	float all_phases = (white_pieces.material - black_pieces.material) + ((white_pieces_eval - black_pieces_eval) * max_pieces_eval);

	if (phase == 0) return all_phases + (white_development - black_development);

	else if (phase == 1) return all_phases;

	else {
		if (white_pieces.material + black_pieces.material <= 90 || (white_pieces.material == 0 || black_pieces.material == 0)) {
			if (white_pieces.material > black_pieces.material || black_pieces.material == 0) {
				all_phases += (-GetDistance(white_pieces.king_sqr, black_pieces.king_sqr) + opposing_king_press[black_pieces.king_sqr]);
			}
			else if (black_pieces.material > white_pieces.material || white_pieces.material == 0) {
				all_phases -= (-GetDistance(white_pieces.king_sqr, black_pieces.king_sqr) + opposing_king_press[white_pieces.king_sqr]);
			}
		}
		return all_phases;
	}
}



// Move rating functions 

int Enginegton::RateKingMove(int dest) {

	int r = 0; 
	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += capture_values[dest_id];

	if (initial_phase < 2) return r - 20; // disincentivize any other move than castling 
	else {
		// assess proximity to the opposing king 
		return r;
	}
}

int Enginegton::RatePawnMove(int dest) {

	int r = 0;
	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += (capture_values[dest_id] * capture_multipliers[0]);
	else r += eval_tables[initial_phase][turn == 1 ? 0 : 6][dest];
	
	// check whether and which pieces this pawn attacks/defends 

	return r;

}

int Enginegton::RateKnightMove(int dest) {

	int r = 0;
	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += (capture_values[dest_id] * capture_multipliers[1]);
	else r += eval_tables[initial_phase][turn == 1 ? 1 : 7][dest]; 

	// check whether and which pieces this knight attacks/defends 
	// forks? 

	return r;
}

int Enginegton::RateBishopMove(int dest) {

	int r = 0;
	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += (capture_values[dest_id] * capture_multipliers[2]);
	else r += eval_tables[initial_phase][turn == 1 ? 2 : 8][dest];

	return r;
}

int Enginegton::RateRookMove(int dest) {

	int r = 0;
	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += (capture_values[dest_id] * capture_multipliers[3]);
	else r += eval_tables[initial_phase][turn == 1 ? 3 : 9][dest];

	return r;
}

int Enginegton::RateQueenMove(int dest) {

	int r = 0;

	int dest_id = GetPieces(-turn).GetPieceID(dest);
	if (dest_id != -1) r += (capture_values[dest_id] * capture_multipliers[4]);

	return r; 
}



void Enginegton::SetupTest(const std::vector<int>& b, int t, int w_castle, int b_castle) {
	turn = t;
	white_pieces = PieceGroup(b, 1, w_castle, zob_table[56][3], zob_table[63][3], zob_table[61][3], zob_table[59][3]);
	black_pieces = PieceGroup(b, -1, b_castle, zob_table[0][9], zob_table[7][9], zob_table[5][9], zob_table[3][9]);

	initial_phase = GetInitialGamePhase();
	if (white_pieces.map.size() > black_pieces.map.size()) max_pieces_eval = 1.0f / ((float)white_pieces.map.size() * 100.0f);
	else max_pieces_eval = 1.0f / ((float)black_pieces.map.size() * 100.0f);

	unsigned long long int hash = 0;
	for (int i = 0; i < 64; ++i) {
		if (b[i] != -1) {
			board_state_bitboard |= (1ULL << i);
			hash ^= zob_table[i][b[i]];
		}
	}
	hash ^= zob_castle[(white_pieces.castling_rights << 2) + black_pieces.castling_rights];
	if (turn == -1) hash ^= zob_turn;

	hash_log.push_back(hash);

	castle_log.push_back(white_pieces.castling_rights);
	castle_log.push_back(black_pieces.castling_rights);
}

void Enginegton::TestSearch(const std::vector<int>& b, int t, int w_castle, int b_castle) {

	SetupTest(b, t, w_castle, b_castle);

	auto start = std::chrono::high_resolution_clock::now();

	Move move = Move();
	MoveQueue q(128);
	MovesGen(q);

	if (turn == 1) {

		float max_eval = std::numeric_limits<float>::lowest();
		float alpha = max_eval;

		while (q.GetSize() > 0 && !early_stop && !timeout) {

			Move next = q.Dequeue();
			float eval = Calculate(1, next, alpha, std::numeric_limits<float>::infinity());
			UndoMove(next);

			if (eval > alpha) alpha = eval;

			if (eval > max_eval) {
				max_eval = eval;
				move = next;
			}
		}
		std::cout << "Final evaluation: " << max_eval << "\n";
	}
	else {

		float min_eval = std::numeric_limits<float>::infinity();
		float beta = min_eval;

		while (q.GetSize() > 0 && !early_stop && !timeout) {

			Move next = q.Dequeue();
			float eval = Calculate(1, next, std::numeric_limits<float>::lowest(), beta);
			UndoMove(next);

			if (eval < beta) beta = eval;

			if (eval < min_eval) {
				min_eval = eval;
				move = next;
			}
		}

		std::cout << "Final evaluation: " << min_eval << "\n";
	}

	float count = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.f;
	std::cout << count << " seconds\n";
	std::cout << "Positions reached: " << pos_count << "\n";
	std::cout << "Checkmates: " << checkmate_count << '\n';
	std::cout << "Transposition table size: " << transposition_table.size() << '\n';

	DebugUINT64(move.move_mask);
}