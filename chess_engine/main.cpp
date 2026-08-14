#include <iostream>
#include <vector>

#include "board/board.hpp"
#include "board/fen.hpp"

#include "movegen/legal.hpp"
#include "search/search.hpp"
#include "uci/uci.hpp"

#include "utils/move_to_string.hpp"

namespace {

enum class RunMode {
    FenCli,
    Uci
};

constexpr RunMode kRunMode = RunMode::FenCli ;

int runFenCli() {
    std::string fen;

    std::cout << "Enter FEN below : \n";
    std::getline(std::cin, fen);

    Board board;
    parseFEN(fen, board);

    // Generate legal moves
    std::vector<Move> moves = generateLegalMoves(board);

    // ================= GAME END =================
    if (moves.empty()) {
        uint64_t kingBB = board.whiteToMove ? board.wk : board.bk;
        if (kingBB == 0) {
            std::cout << "Invalid position!\n";
            return 0;
        }

        int kingSq = __builtin_ctzll(kingBB);

        if (isSquareAttacked(board, kingSq, !board.whiteToMove)) {
          std::cout << "Checkmate!\n";
        } else {
          std::cout << "Stalemate!\n";
        }

        return 0;
    }

    // ================= SEARCH =================
    int depth = 4; // you can change

    Move bestMove = findBestMove(board, depth);

    std::cout << "Best Move: " << moveToString(bestMove) << "\n";

    return 0;
}

} // namespace

int main() {
    if (kRunMode == RunMode::Uci) {
        runUciLoop(std::cin, std::cout);
        return 0;
    }

    return runFenCli();
}
