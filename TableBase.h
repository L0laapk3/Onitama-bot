#pragma once

#include <array>
#include <map>
#include "Botama.h"
#include "Board.h"
#include "CardBoard.h"
#include "sparsehash/dense_hash_map.h"
#include "flat_hash_map/bytell_hash_map.hpp"


// generating the tablebase is not thread safe!! for now
namespace TableBase {

#pragma pack (push)
#pragma pack (1)
	struct BoardValue {
		BoardValue(const std::pair<Board, uint8_t>& pair);
		uint32_t boardComp; // max 6 men
		uint8_t DTW;
	};
#pragma pack (pop)

	typedef ska::bytell_hash_map<Board, uint8_t, BoardHash> MapType;
	// typedef google::dense_hash_map<Board, uint8_t, BoardHash> MapType;

	extern MapType wonOddBoards;
	extern MapType wonEvenBoards;

	template<bool isMine>
	void addToTables(const GameCards& gameCards, const Board& board, const bool finished = false);

	bool singleDepth(const GameCards& gameCards);
	
	template<bool templeWin>
	void placePieces(const GameCards& gameCards, U64 pieces, std::array<U32, 2> occupied, U32 beforeKing, U32 beforeOtherKing, U32 startAt, U32 spotsLeft, U32 minSpots0, U32 minSpotsAll, U32 myMaxPawns, U32 otherMaxPawns);
	void placePiecesTemple(const GameCards& gameCards, const Board& board, const bool finished);
	void placePiecesDead(const GameCards& gameCards, const Board& board, const bool finished);
	uint8_t generate(const GameCards& gameCards, const U32 maxMen);
	uint8_t generate(const GameCards& gameCards, const U32 maxMen, const U32 maxMenPerSide);
}