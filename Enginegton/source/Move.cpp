#include "pch.h"
#include "Move.h"

#include <iostream>

// for displaying a uint64_t board in the console 
void DebugUINT64(uint64_t val) {
	for (int i = 0; i < 64; ++i) {
		uint64_t mask = 1ULL << i;
		uint64_t bit = (val & mask) >> i;
		std::cout << bit << " ";
		if ((i + 1) % 8 == 0) std::cout << "\n";
	}
	std::cout << "\n";
}

std::string OutUINT64(uint64_t val) {
	std::string out = "";
	for (int i = 0; i < 64; ++i) {
		uint64_t mask = 1ULL << i;
		uint64_t bit = (val & mask) >> i;
		if (bit > 0) out += "1";
		else out += "0";
		out += " ";
		if ((i + 1) % 8 == 0) out += "\n";
	}
	return out;
}

// gets manhattan distance between sqr1 and sqr2 (used in incentivizing a king to approach the other king in the simple rook/queen chackmating pattern)
int GetDistance(int sqr1, int sqr2) {

	int sqr1_file = sqr1 % 8;
	int sqr1_rank = (sqr1 - sqr1_file) >> 3;
	int sqr2_file = sqr2 % 8;
	int sqr2_rank = (sqr2 - sqr2_file) >> 3;

	return abs(sqr1_file - sqr2_file) + abs(sqr1_rank - sqr2_rank);
}

uint64_t PlayerPawnControl(int y, int x) {

	uint64_t control = 0;
	int origin = y * 8 + x;
	if (x + 1 < 8) control ^= (1ULL << origin - 7);
	if (x - 1 >= 0) control ^= (1ULL << origin - 9);

	return control;
}

uint64_t OpponentPawnControl(int y, int x) {

	uint64_t control = 0;
	int origin = y * 8 + x;
	if (x - 1 >= 0) control ^= (1ULL << origin + 7);
	if (x + 1 < 8) control ^= (1ULL << origin + 9);

	return control;
}

uint64_t PlayerPawnPush(int y, int x) {
	uint64_t push = 0;
	int origin = y * 8 + x;
	push ^= (1ULL << origin - 8);
	if (y == 6) push ^= (1ULL << origin - 16); 
	return push; 
}

uint64_t OpponentPawnPush(int y, int x) {
	uint64_t push = 0;
	int origin = y * 8 + x;
	push ^= (1ULL << origin + 8);
	if (y == 1) push ^= (1ULL << origin + 16);
	return push;
}

uint64_t KingControl(int y, int x) {
	uint64_t control = 0;
	int origin = y * 8 + x;

	if (x + 1 < 8) control ^= (1ULL << origin + 1);
	if (x - 1 >= 0) control ^= (1ULL << origin - 1);
	if (y - 1 >= 0) control ^= (1ULL << origin - 8);
	if (y + 1 < 8) control ^= (1ULL << origin + 8);
	if (x - 1 >= 0 && y - 1 >= 0) control ^= (1ULL << origin - 9);
	if (x + 1 < 8 && y + 1 < 8) control ^= (1ULL << origin + 9);
	if (x - 1 >= 0 && y + 1 < 8) control ^= (1ULL << origin + 7);
	if (x + 1 < 8 && y - 1 >= 0) control ^= (1ULL << origin - 7);

	return control;
}

uint64_t KingArea(int y, int x) {
	uint64_t area = 0;

	int origin = y * 8 + x;

	if (x + 2 < 8) area ^= (1ULL << origin + 2);
	if (x - 2 >= 0) area ^= (1ULL << origin - 2);
	if (y - 2 >= 0) area ^= (1ULL << origin - 16);
	if (y + 2 < 8) area ^= (1ULL << origin + 16);
	if (x - 2 >= 0 && y - 2 >= 0) area ^= (1ULL << origin - 18);
	if (x + 2 < 8 && y + 2 < 8) area ^= (1ULL << origin + 18);
	if (x - 2 >= 0 && y + 2 < 8) area ^= (1ULL << origin + 14);
	if (x + 2 < 8 && y - 2 >= 0) area ^= (1ULL << origin - 14);

	return area;
}

uint64_t KnightControl(int y, int x) {
	uint64_t control = 0;
	int origin = y * 8 + x;

	if (x + 2 < 8 && y + 1 < 8) control ^= (1ULL << origin + 10);
	if (x - 2 >= 0 && y + 1 < 8) control ^= (1ULL << origin + 6);
	if (x + 2 < 8 && y - 1 >= 0) control ^= (1ULL << origin - 6);
	if (x - 2 >= 0 && y - 1 >= 0) control ^= (1ULL << origin - 10);
	if (y + 2 < 8 && x + 1 < 8) control ^= (1ULL << origin + 17);
	if (y - 2 >= 0 && x + 1 < 8) control ^= (1ULL << origin - 15);
	if (y + 2 < 8 && x - 1 >= 0) control ^= (1ULL << origin + 15);
	if (y - 2 >= 0 && x - 1 >= 0) control ^= (1ULL << origin - 17);

	return control;
}

uint64_t RookFreeControl(int y, int x) {

	uint64_t control = 0;

	for (int i = y - 1; i >= 0; i--) control ^= (1ULL << i * 8 + x);
	for (int i = y + 1; i < 8; i++) control ^= (1ULL << i * 8 + x);
	for (int i = x + 1; i < 8; i++) control ^= (1ULL << y * 8 + i);
	for (int i = x - 1; i >= 0; i--) control ^= (1ULL << y * 8 + i);
	
	return control;
}

uint64_t BishopFreeControl(int y, int x) {
	
	uint64_t control = 0;

	for (int i = y - 1, j = x + 1; i >= 0 && j < 8; i--, j++) control ^= (1ULL << i * 8 + j);
	for (int i = y + 1, j = x + 1; i < 8 && j < 8; i++, j++) control ^= (1ULL << i * 8 + j);
	for (int i = y - 1, j = x - 1; i >= 0 && j >= 0; i--, j--) control ^= (1ULL << i * 8 + j);
	for (int i = y + 1, j = x - 1; i < 8 && j >= 0; i++, j--) control ^= (1ULL << i * 8 + j);
	
	return control;

}

uint64_t RookMask(int origin) {

	int x = origin % 8;
	int y = (origin - x) >> 3;

	uint64_t control = 0;

	for (int i = y - 1; i >= 1; i--) control ^= (1ULL << i * 8 + x);
	for (int i = y + 1; i < 7; i++) control ^= (1ULL << i * 8 + x);
	for (int i = x + 1; i < 7; i++) control ^= (1ULL << y * 8 + i);
	for (int i = x - 1; i >= 1; i--) control ^= (1ULL << y * 8 + i);

	return control;
}

uint64_t BishopMask(int origin) {

	int x = origin % 8;
	int y = (origin - x) >> 3;

	uint64_t control = 0;

	for (int i = y - 1, j = x + 1; i >= 1 && j < 7; i--, j++) control ^= (1ULL << i * 8 + j);
	for (int i = y + 1, j = x + 1; i < 7 && j < 7; i++, j++) control ^= (1ULL << i * 8 + j);
	for (int i = y - 1, j = x - 1; i >= 1 && j >= 1; i--, j--) control ^= (1ULL << i * 8 + j);
	for (int i = y + 1, j = x - 1; i < 7 && j >= 1; i++, j--) control ^= (1ULL << i * 8 + j);

	return control;
}

uint64_t RookAttack(int origin, uint64_t blockers) {

	int x = origin % 8;
	int y = (origin - x) >> 3;

	uint64_t attack = 0;

	for (int i = y - 1; i >= 0; i--) {
		uint64_t next = (1ULL << i * 8 + x);
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = y + 1; i < 8; i++) {
		uint64_t next = (1ULL << i * 8 + x);
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = x + 1; i < 8; i++) {
		uint64_t next = (1ULL << y * 8 + i);
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = x - 1; i >= 0; i--) {
		uint64_t next = (1ULL << y * 8 + i);
		attack |= next;
		if ((blockers & next) > 0) break;
	}
	return attack;
}

uint64_t BishopAttack(int origin, uint64_t blockers) {

	uint64_t attack = 0;

	int x = origin % 8;
	int y = (origin - x) >> 3;

	for (int i = y - 1, j = x + 1; i >= 0 && j < 8; i--, j++) {
		uint64_t next = 1ULL << i * 8 + j;
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = y + 1, j = x + 1; i < 8 && j < 8; i++, j++) {
		uint64_t next = 1ULL << i * 8 + j;
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = y - 1, j = x - 1; i >= 0 && j >= 0; i--, j--) {
		uint64_t next = 1ULL << i * 8 + j;
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	for (int i = y + 1, j = x - 1; i < 8 && j >= 0; i++, j--) {
		uint64_t next = 1ULL << i * 8 + j;
		attack |= next;
		if ((blockers & next) > 0) break;
	}

	return attack;
}

std::vector<int> GetIndices(uint64_t move_mask) {

	std::vector<int> indices; 
	int bit = 0;
	while (move_mask > 0) {
		uint64_t move = move_mask & (1ULL << bit);
		if (move > 0) {
			indices.push_back(bit);
			move_mask ^= move;
		}
		++bit; 
	}
	return indices;
}


void MoveQueue::Enqueue(const Move& m) {
	moves.push_back(m);
	++N;
	QueueUp(N);
}

Move MoveQueue::Dequeue() {

	std::swap(moves[1], moves[N]);
	Move val = std::move(moves[N]);
	moves.pop_back();
	--N;
	QueueDown(1);

	return val;
}

void MoveQueue::QueueUp(int i) {
	if (i > N || i == 1) return;

	int index = GetParent(i);
	if (moves[i].rating > moves[index].rating) std::swap(moves[index], moves[i]);
	QueueUp(index);
}

void MoveQueue::QueueDown(int i) {
	if (i > N) return;

	int prev = i;
	if (GetLeftChild(i) <= N && moves[i].rating < moves[GetLeftChild(i)].rating) prev = GetLeftChild(i);
	if (GetRightChild(i) <= N && moves[GetRightChild(i)].rating > moves[prev].rating) prev = GetRightChild(i);

	if (prev != i) {
		std::swap(moves[i], moves[prev]);
		QueueDown(prev);
	}
}