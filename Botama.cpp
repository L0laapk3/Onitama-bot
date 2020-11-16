// notlosebot.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "Board.h"
#include "Card.h"
#include "CardBoard.h"
#include <string>
#include <bitset>
#include <chrono>
#include <algorithm>

U64 count = 0;
void recursive(GameCards& gameCards, const Board& board, const bool finished, U32 depth) {
	if (!depth || finished)
		count++;
	else
		board.forwardMoves<*recursive>(gameCards, depth - 1);
}
void recursiveCheat(GameCards& gameCards, const Board& board, const bool finished, U32 depth) {
	if (finished)
		count++;
	else if (depth == 1)
		count += board.countForwardMoves(gameCards);
	else
		board.forwardMoves<*recursiveCheat>(gameCards, depth - 1);
}

int main() {
	GameCards gameCards = CardBoard::fetchGameCards({ "ox", "boar", "horse", "elephant", "crab" });
	Board board = Board::fromString("1121100000000000000033433", false);
	//Board board = Board::fromString("1020101010000000303030403", true);
	//Board board = Board::fromString("0031000100342101000300300", true);
	board.print(gameCards);

	for (int depth = 1; depth < 10; depth++) {
		auto start = std::chrono::steady_clock::now();
		count = 0;
		recursive(gameCards, board, false, depth);
		auto end = std::chrono::steady_clock::now();
		float nps = std::roundf(count / (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() * .001f));
		std::cout << depth << "  " << nps << "M/s \t" << count << std::endl;
	}
}