﻿
#include "Board.h"

#include <bitset>
#include <iostream>
#include <intrin.h>
#include <algorithm>

#pragma intrinsic(_BitScanForward)


constexpr std::array<uint32_t, 2> CARDS_PLAYERMASK{ 0xffULL, 0xff00ULL };
constexpr uint32_t CARDS_SWAPMASK = 0xff0000ULL;


Board::Board(std::string str) {
	for (int i = 0; i < 25; i++) {
		if (str[i] == '1' || str[i] == '2')
			pieces[0] |= 1 << i;
		if (str[i] == '3' || str[i] == '4')
			pieces[1] |= 1 << i;
		if (str[i] == '2' || str[i] == '4')
			kings |= 1 << i;
	}

	unsigned long i = 4;
	std::cout << (int)_BitScanForward(&i, kings) << " ";
	std::cout << i << "\n";
}


Board::Board(std::array<uint32_t, 2> pieces, uint32_t kings, bool player, uint32_t cards) : pieces{ pieces }, kings{ kings }, player{ player }, cards{ cards } { }
Board::Board(const Board& board) : pieces{ board.pieces }, kings{ board.kings }, player{ board.player }, cards{ board.cards } { };


std::string cardsShortName(std::array<const CardBoard, 5>& gameCards, uint32_t bits, unsigned long long length) {
	bits &= 0xff;
	unsigned long i;
	std::string res = "";
	while (_BitScanForward(&i, bits)) {
		bits &= ~(1 << i);
		const std::string& card = i < 5 ? gameCards[i].name : (std::to_string(i) + "??");
		for (uint32_t i = 0; i < length; i++)
			res += card.size() > i ? card[i] : ' ';
		res += ' ';
	}
	return res;
}
void Board::print(std::array<const CardBoard, 5>& gameCards) const {
	Board::print(gameCards, { *this });
}
void Board::print(std::array<const CardBoard, 5>& gameCards, std::vector<Board> boards) {
	constexpr size_t MAXPERLINE = 10;
	for (size_t batch = 0; batch < boards.size(); batch += MAXPERLINE) {
		for (size_t i = batch; i < std::min(batch + MAXPERLINE, boards.size()); i++) {
			const Board& board = boards[i];
			std::cout << cardsShortName(gameCards, board.cards >> 8, 4) << ' ';
		}
		std::cout << std::endl;
		for (int r = 5; r-- > 0;) {
			for (size_t i = batch; i < std::min(batch + MAXPERLINE, boards.size()); i++) {
				const Board& board = boards[i];
				const auto swapCardName = cardsShortName(gameCards, board.cards >> 16, 5);
				std::cout << " |";
				for (int c = 0; c < 5; c++) {
					const int mask = 1 << (5 * r + c);
					if (board.pieces[0] & board.pieces[1] & mask)
						std::cout << ((board.kings & mask) ? 'e' : '?');
					else if (board.pieces[0] & mask)
						std::cout << ((board.kings & mask) ? '0' : 'o');
					else if (board.pieces[1] & mask)
						std::cout << ((board.kings & mask) ? 'X' : '+');
					else
						std::cout << ((board.kings & mask) ? '!' : ' ');
				}
				if (swapCardName.size() < 5)
					std::cout << "|   ";
				else
					std::cout << '|' << swapCardName[4ULL - r] << "  ";
			}
			std::cout << std::endl;
		}
		for (size_t i = batch; i < std::min(batch + MAXPERLINE, boards.size()); i++) {
			const Board& board = boards[i];
			std::cout << cardsShortName(gameCards, board.cards, 4) << ' ';
		}
		std::cout << std::endl;
	}
}

void printCards(std::vector<uint32_t> cardsVec) {
	for (auto& cards : cardsVec)
		std::cout << std::bitset<8>(cards >> 16) << ' ' << std::bitset<8>(cards >> 8) << ' ' << std::bitset<8>(cards) << "  ";
	std::cout << std::endl;
}


void Board::iterateMoves(const CardBoard& card, uint32_t newCards, bool movingPlayer, MoveFunc& cb) const {
	card.print();
	uint32_t playerPieces = pieces[movingPlayer];
	unsigned long from;
	while (_BitScanForward(&from, playerPieces)) {
		playerPieces &= ~(1 << from);
		std::array<uint32_t, 2> boardsWithoutPiece{ pieces };
		boardsWithoutPiece[movingPlayer] &= ~(1 << from);
		uint32_t kingsWithoutPiece = kings & ~(1 << from);
		uint32_t newKingMask = kingsWithoutPiece == kings ? 0 : ~0;
		unsigned long to;
		uint32_t scan = card.moveBoard[player][from] & ~pieces[movingPlayer];
		while (_BitScanForward(&to, scan)) {
			scan &= ~(1 << to);
			std::array<uint32_t, 2> newBoards{ boardsWithoutPiece };
			newBoards[movingPlayer] |= (1 << to);
			cb(Board(newBoards, kingsWithoutPiece | ((1 << to) & newKingMask), !player, newCards));
		}
	}
}

void Board::forwardMoves(std::array<const CardBoard, 5>& gameCards, MoveFunc cb) const {
	uint32_t cardScan = cards & CARDS_PLAYERMASK[player];
	for (int i = 0; i < 2; i++) {
		unsigned long cardI;
		_BitScanForward(&cardI, cardScan);
		cardScan &= ~(1ULL << cardI);
		uint32_t newCards = (cards & ~CARDS_SWAPMASK & ~(1 << cardI)) | ((cards & CARDS_SWAPMASK) >> (player ? 8 : 16)) | (1 << (cardI + (player ? 8 : 16)));
		const auto& card = gameCards[cardI & 7];
		iterateMoves(card, newCards, player, cb);
	}
}

void Board::reverseMoves(std::array<const CardBoard, 5>& gameCards, MoveFunc cb) const {
	unsigned long swapCardI;
	_BitScanForward(&swapCardI, cards & CARDS_SWAPMASK);
	uint32_t cardScan = cards & CARDS_PLAYERMASK[!player];
	unsigned long playerCardI;
	_BitScanForward(&playerCardI, cardScan);
	uint32_t swapCardPlayerMask = (cards & CARDS_SWAPMASK) >> (!player ? 8 : 16);
	uint32_t firstCard = 1ULL << playerCardI;
	uint32_t secondCard = cards & CARDS_PLAYERMASK[!player] & ~firstCard;
	uint32_t newCards = (cards & 0xffffULL) | swapCardPlayerMask;
	iterateMoves(gameCards[swapCardI & 7], (newCards & ~firstCard) | (firstCard << (!player ? 8 : 16)), !player, cb);
	iterateMoves(gameCards[swapCardI & 7], (newCards & ~secondCard) | (secondCard << (!player ? 8 : 16)), !player, cb);
}