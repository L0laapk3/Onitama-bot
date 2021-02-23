
#include "Game.h"
#include <algorithm>
#include <chrono>

#include "TableBase.h"



SearchResult Game::search(const Board& board, S32 maxDepth, const bool quiescent, Score alpha, const Score beta) {
	return quiescent ? search<true>(board, maxDepth, alpha, beta) : search<false>(board, maxDepth, alpha, beta);
}

template<bool quiescent>
SearchResult Game::search(const Board& board, S32 maxDepth, Score alpha, const Score beta) {
	// negamax with alpha beta pruning
	bool player = board.pieces & MASK_TURN;

	maxDepth--;

	U64 total = 1;

	Board bestBoard{ 0 };


	auto type = TranspositionTable::EntryType::Alpha;
	bool first = true;

	const auto iterateBoard = [&](const Board& newBoard) {
		Score childScore = SCORE_MIN;
		if (newBoard.pieces & (1 << INDEX_FINISHED)) {
			childScore = SCORE_WIN + maxDepth;
			total++;
		} else {
			bool TBHit = false;
				
			if (_popcnt32(newBoard.pieces & MASK_PIECES) <= 3 && _popcnt64(newBoard.pieces & (MASK_PIECES << 32)) <= 3) {
				const int8_t result = player ? (int8_t)(*TableBase::table)[TableBase::compress6Men(newBoard)] : -(int8_t)(*TableBase::table)[TableBase::invertCompress6Men(newBoard)];
				if (result != 0) {
					uint16_t depth = (result > 0 ? result : -result) << 1;
					Score score = SCORE_WIN + maxDepth - depth;
					if (result < 0 == player)
						childScore = score;
					else
						childScore = -score;
					TBHit = true;
				}
			}
			if (!TBHit) {

				SearchResult childSearch;
				if (first) // PVS
					childSearch = search(newBoard, maxDepth, !maxDepth || quiescent, -beta, -alpha);
				else {
					childSearch = search(newBoard, maxDepth, !maxDepth || quiescent, -alpha - 1, -alpha);
					if (alpha < -childSearch.score && -childSearch.score < beta) // PVS failed
						childSearch = search(newBoard, maxDepth, !maxDepth || quiescent, -beta, childSearch.score);
				}
				childScore = -childSearch.score;
				total += childSearch.total;
			}
		}
		first = false;

		if (childScore > alpha)
			bestBoard = newBoard;
		if (childScore >= beta) {
			type = TranspositionTable::EntryType::Beta;
			alpha = beta;
			return true;
		}
		if (childScore > alpha) {
			type = TranspositionTable::EntryType::Exact;
			alpha = childScore;
		}
		return false;
	};


	U64 historyBest = 0;
	TranspositionTable::Entry* history;
	if (quiescent) {
		Score standingPat = (player ? -1 : 1) * board.eval(cards);
		if (standingPat >= beta)
			return { beta, 0, total };
		if (alpha < standingPat)
			alpha = standingPat;
	} else {
		history = transpositionTable.get(board);
		if (history != nullptr && history->depth >= maxDepth) { // tt hit
			if (history->type == TranspositionTable::EntryType::Exact)
				return { history->score, history->bestMove, total };
			if (history->type == TranspositionTable::EntryType::Alpha && history->score <= alpha)
				return { alpha, history->bestMove, total };
			if (history->type == TranspositionTable::EntryType::Beta && history->score >= beta)
				return { beta, history->bestMove, total };
			if (history->bestMove.pieces) {
				bestBoard = history->bestMove;	
				if (_popcnt64((bestBoard.pieces ^ board.pieces) & (MASK_PIECES | (MASK_PIECES << 32))) > 3) {
					std::cout << std::bitset<64>(bestBoard.pieces) << std::endl;
					std::cout << "at ttread" << std::endl;
					board.print(cards);
					bestBoard.print(cards);
					assert(_popcnt64((bestBoard.pieces ^ board.pieces) & (MASK_PIECES | (MASK_PIECES << 32))) <= 3);
				}
				if (iterateBoard(bestBoard))
					goto pruneLoop;
				historyBest = history->bestMove.pieces;
			}
		}
	}

	
	// Ive spent too many hours trying to get generator functions to compile since i want lazy move generation
	// fuck it, just copy pasting the movegen in here
	const CardsPos& cardsPos = CARDS_LUT[(board.pieces & MASK_CARDS) >> INDEX_CARDS];
	U64 piecesWithoutCards = board.pieces & ~MASK_CARDS;
	piecesWithoutCards ^= MASK_TURN; // invert player bit
	for (int taking = 1; taking >= (quiescent ? 1 : 0); taking--) {
		U32 cardStuff = cardsPos.players[player];
		const U32 takeMask = ~(board.pieces >> (player ? 32 : 0)) & (board.pieces >> (player ? 0 : 32) ^ (taking ? 0 : ~0));
		const U32 kingMask = ~(board.pieces >> (player ? 32 : 0)) & ((board.pieces >> (player ? 0 : 32) | MASK_END_POSITIONS[player]) ^ (taking ? 0 : ~0));
		for (int i = 0; i < 2; i++) {
			unsigned long cardI = cardStuff & 0xff;
			U64 piecesWithNewCards = piecesWithoutCards | (((U64)cardStuff & 0xff00) << (INDEX_CARDS - 8ULL));
			cardStuff >>= 16;
			const auto& moveBoard = cards[cardI].moveBoards[player];
			U32 bitScan = (piecesWithNewCards >> (player ? 32 : 0)) & MASK_PIECES;
			U32 kingPieceNum = (piecesWithNewCards >> INDEX_KINGS[player]) & 7;
			const U32 opponentKingPieceNum = (piecesWithNewCards >> INDEX_KINGS[!player]) & 7;
			const U32 beforeKingMask = _pdep_u32(1ULL << kingPieceNum, piecesWithNewCards >> (player ? 32 : 0)) - 1;
			const U32 opponentKing = _pdep_u32(1ULL << opponentKingPieceNum, piecesWithNewCards >> (player ? 0 : 32));
			const U32 opponentBeforeKingPieces = (piecesWithNewCards >> (player ? 0 : 32)) & (opponentKing - 1);
			piecesWithNewCards &= ~(((U64)0b1111111) << INDEX_KINGS[0]);

			unsigned long fromI;
			while (_BitScanForward(&fromI, bitScan)) {
				bool isKingMove = !kingPieceNum;
				U32 scan = moveBoard[fromI] & (isKingMove ? kingMask : takeMask);
				const U32 fromBit = (1ULL << fromI);
				bitScan -= fromBit;
				U64 newPiecesWithoutLandPiece = piecesWithNewCards & ~(((U64)fromBit) << (player ? 32 : 0));

				const U32 endMask = opponentKing | (isKingMove ? MASK_END_POSITIONS[player] : 0);
				while (scan) {
					const U32 landBit = scan & -scan;
					scan -= landBit;
					Board newBoard = Board{ newPiecesWithoutLandPiece };
					newBoard.pieces |= ((U64)landBit) << (player ? 32 : 0);	 // add arrival piece
					newBoard.pieces &= ~(((U64)landBit) << (player ? 0 : 32)); // possible take piece
					const U32 beforeKingPieces = (newBoard.pieces >> (player ? 32 : 0)) & (isKingMove ? landBit - 1 : beforeKingMask);
					newBoard.pieces |= ((U64)_popcnt32(beforeKingPieces)) << INDEX_KINGS[player];
					newBoard.pieces |= ((U64)_popcnt32(opponentBeforeKingPieces & ~landBit)) << INDEX_KINGS[!player];
					newBoard.pieces |= landBit & endMask ? 1 << INDEX_FINISHED : 0;
					if (newBoard.pieces == historyBest)
						continue;
					// end of movegen
					// beginning of negamax
					
					if (iterateBoard(newBoard))
						goto pruneLoop;

					// end of negamax
					// continue movegen
				}
				kingPieceNum--;
			}
		}
	}
	pruneLoop:

	if (!quiescent) {
		// if (_popcnt64((bestBoard.pieces ^ board.pieces) & (MASK_PIECES | (MASK_PIECES << 32))) > 3) {
		// 	board.print(cards);
		// 	bestBoard.print(cards);
		// 	assert(_popcnt64((bestBoard.pieces ^ board.pieces) & (MASK_PIECES | (MASK_PIECES << 32))) <= 3);
		// }
		transpositionTable.add(board, bestBoard, alpha, maxDepth, type);
	}
	return { alpha, bestBoard, total };
}



// // only works at finding wins for blue, made for TB gen
// bool Board::searchWinIn(const GameCards& gameCards, const U16 depth) const {
// 	bool player = pieces & MASK_TURN;
// 	if (depth == 1)
// 		return winInOne(gameCards);


// 	// same story as before, just copied from there..
// 	const CardsPos& cardsPos = CARDS_LUT[(pieces & MASK_CARDS) >> INDEX_CARDS];
// 	U64 piecesWithoutCards = pieces & ~MASK_CARDS;
// 	piecesWithoutCards ^= MASK_TURN; // invert player bit
// 	for (int taking = 1; taking >= 0; taking--) {
// 		U32 cardStuff = cardsPos.players[player];
// 		const U32 takeMask = ~(pieces >> (player ? 32 : 0)) & (pieces >> (player ? 0 : 32) ^ (taking ? 0 : ~0));
// 		const U32 kingMask = MASK_END_POSITIONS[player] ^ (taking ? 0 : ~0);
// 		for (int i = 0; i < 2; i++) {
// 			unsigned long cardI = cardStuff & 0xff;
// 			U64 piecesWithNewCards = piecesWithoutCards | (((U64)cardStuff & 0xff00) << (INDEX_CARDS - 8ULL));
// 			cardStuff >>= 16;
// 			const auto& moveBoard = gameCards[cardI].moveBoards[player];
// 			U32 bitScan = (piecesWithNewCards >> (player ? 32 : 0)) & MASK_PIECES;
// 			U32 kingPieceNum = (piecesWithNewCards >> INDEX_KINGS[player]) & 7;
// 			const U32 opponentKingPieceNum = (piecesWithNewCards >> INDEX_KINGS[!player]) & 7;
// 			const U32 beforeKingMask = _pdep_u32(1ULL << kingPieceNum, piecesWithNewCards >> (player ? 32 : 0)) - 1;
// 			const U32 opponentKing = _pdep_u32(1ULL << opponentKingPieceNum, piecesWithNewCards >> (player ? 0 : 32));
// 			const U32 opponentBeforeKingPieces = (piecesWithNewCards >> (player ? 0 : 32)) & (opponentKing - 1);
// 			piecesWithNewCards &= ~(((U64)0b1111111) << INDEX_KINGS[0]);

// 			unsigned long fromI;
// 			while (_BitScanForward(&fromI, bitScan)) {
// 				bool isKingMove = !kingPieceNum;
// 				U32 scan = moveBoard[fromI] & (isKingMove ? kingMask : takeMask);
// 				const U32 fromBit = (1ULL << fromI);
// 				bitScan -= fromBit;
// 				U64 newPiecesWithoutLandPiece = piecesWithNewCards & ~(((U64)fromBit) << (player ? 32 : 0));

// 				const U32 endMask = opponentKing | (isKingMove ? MASK_END_POSITIONS[player] : 0);
// 				while (scan) {
// 					const U32 landBit = scan & -scan;
// 					scan -= landBit;
// 					Board board = Board{ newPiecesWithoutLandPiece };
// 					board.pieces |= ((U64)landBit) << (player ? 32 : 0);	 // add arrival piece
// 					board.pieces &= ~(((U64)landBit) << (player ? 0 : 32)); // possible take piece
// 					const U32 beforeKingPieces = (board.pieces >> (player ? 32 : 0)) & (isKingMove ? landBit - 1 : beforeKingMask);
// 					board.pieces |= ((U64)_popcnt32(beforeKingPieces)) << INDEX_KINGS[player];
// 					board.pieces |= ((U64)_popcnt32(opponentBeforeKingPieces & ~landBit)) << INDEX_KINGS[!player];
// 					const bool finished = landBit & endMask;
// 					// end of movegen
// 					// beginning of recursive searchWinIn
// 					if (finished)
// 						return true;
// 					if (board.searchWinIn(gameCards, depth - 1) != player)
// 						return !player;

// 					// end of recursive searchWinIn
// 					// continue movegen
// 				}
// 				kingPieceNum--;
// 			}
// 		}
// 	}
// 	return player;
// }





constexpr U32 startDepth = 0;
constexpr U32 minDepth = 4;
SearchResult Game::searchTime(const Board& board, const U32 turn, const U64 timeBudget, const int verboseLevel, const int expectedDepth) {
	if (verboseLevel >= 2)
		board.print(cards);
	auto lastTime = 1ULL;
	auto predictedTime = 1ULL;
	S32 depth = startDepth;
	S32 shortestEnd = std::numeric_limits<S32>::max();
	SearchResult result;
	while (true) {
		const auto beginTime = std::chrono::steady_clock::now();
		++depth;
		result = search(board, depth);
		const auto time = std::max(1ULL, (unsigned long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - beginTime).count());
		predictedTime = time * time / lastTime;
		lastTime = time;
		bool foundWin = std::abs(result.score) >= SCORE_WIN - SCORE_QUIESCENCE_WIN_OFFSET / 2;
		bool foundProbableWin = std::abs(result.score) >= SCORE_WIN - SCORE_QUIESCENCE_WIN_OFFSET * 2;

		bool wrongWinDepth = false;
		S32 end = depth - (std::abs(result.score) - (foundWin ? SCORE_WIN : SCORE_WIN - SCORE_QUIESCENCE_WIN_OFFSET));
		if (foundWin) {
			shortestEnd = std::min(end, shortestEnd);
			if (expectedDepth >= 0 && end != expectedDepth) {
				board.print(cards);
				wrongWinDepth = true;
			}
		}

		bool TBWin = depth + 1 < end;
		bool lastIteration = ((predictedTime > timeBudget * 1000) && (depth >= minDepth)) || (depth >= 64) || ((shortestEnd - 1) <= depth) || (foundWin && TBWin && depth <= 2);

		if ((verboseLevel >= 1 && lastIteration) || verboseLevel >= 2) {
			if (result.score == SCORE_MIN) {
				printf("No moves found.");
				return result;
			}
			if (timeBudget >= 1000)
				printf("%2i. depth %2i in %.2fs (%9llu, %2lluM/s, EBF=%5.2f): ", turn, depth, (float)time / 1E6, result.total, result.total / time, std::pow(result.total, 1. / depth));
			else if (timeBudget >= 10)
				printf("%2i. depth %2i in %3.0fms (%9llu, %2lluM/s, EBF=%5.2f): ", turn, depth, (float)time / 1E3, result.total, result.total / time, std::pow(result.total, 1. / depth));
			else
				printf("%2i. depth %2i in %.1fms (%9llu, %2lluM/s, EBF=%5.2f): ", turn, depth, (float)time / 1E3, result.total, result.total / time, std::pow(result.total, 1. / depth));
			if (foundProbableWin) {
				bool isLoss = result.score < 0;
				std::cout << (isLoss ? "lose" : "win") << " in " << ((end & ~1) | !isLoss);
				if (!foundWin)
					std::cout << '?';
				// else if (TBWin)
				// 	std::cout << '*';
				if (wrongWinDepth) {
					std::cout << " (expected " << expectedDepth << ")" << std::endl;
					assert(0);
				}
					std::cout << std::endl;
			} else if (verboseLevel >= 1)
				printf("%.2f\n", (float)result.score / SCORE_PIECE);
		}
		if (lastIteration)
			break;
	}
	if (verboseLevel >= 2)
		std::cout << std::endl;
	transpositionTable.report();
	return result;
}
