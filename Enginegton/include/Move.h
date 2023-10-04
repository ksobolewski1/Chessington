#pragma once

#include <algorithm>
#include <iostream>
#include <vector>

void DebugUINT64(uint64_t val);

std::string OutUINT64(uint64_t val);

int GetDistance(int sqr1, int sqr2);

struct Move {

	int origin;
	int destination;
	int type; 
	int rating; 

	uint64_t move_mask;

	Move() = default;
	Move(uint64_t mm, int o, int d, int r, int t = 0) : move_mask(mm), origin(o), destination(d), rating(r), type(t) {}
};

// control and unrestricted move masks 
uint64_t PlayerPawnControl(int y, int x);
uint64_t OpponentPawnControl(int y, int x);
uint64_t PlayerPawnPush(int y, int x);
uint64_t OpponentPawnPush(int y, int x);
uint64_t KingControl(int y, int x);
uint64_t KingArea(int y, int x);
uint64_t KnightControl(int y, int x);
uint64_t RookFreeControl(int y, int x);
uint64_t BishopFreeControl(int y, int x);

// for rook and bishop magics 
uint64_t RookMask(int origin);
uint64_t BishopMask(int origin);
uint64_t RookAttack(int origin, uint64_t blockers);
uint64_t BishopAttack(int origin, uint64_t blockers);

const int rook_index_shifts[64] = {
	12, 11, 11, 11, 11, 11, 11, 12,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	12, 11, 11, 11, 11, 11, 11, 12
};

const int bishop_index_shifts[64] = {
	6, 5, 5, 5, 5, 5, 5, 6,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 5, 5, 5, 5, 5, 5, 6
};


// for BitScan functions, so that they only iterate over possible bits
// It is also used in getting rook and bishop attacks 
std::vector<int> GetIndices(uint64_t move_mask);


// An adapted max binary heap for sorting moves based on their rating 
struct MoveQueue {

private:

	std::vector<Move> moves;
	int N = 0;

	int GetParent(int index) { return index >> 1; }
	int GetLeftChild(int index) { return index << 1; }
	int GetRightChild(int index) { return (index << 1) + 1; }

	void QueueUp(int i);
	void QueueDown(int i);

public:

	MoveQueue(size_t res) : moves(std::vector<Move>{Move()}), N(0) { moves.reserve(res); }
	int GetSize() { return N; }

	void Enqueue(const Move& m);
	Move Dequeue();
};
