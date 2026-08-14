#include <array>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "../board/board.hpp"
#include "../board/fen.hpp"
#include "../move/makemove.hpp"
#include "../movegen/legal.hpp"
#include "../search/search.hpp"
#include "../utils/move_to_string.hpp"

namespace {

bool sameBoard(const Board& a, const Board& b) {
    return a.wp == b.wp && a.wn == b.wn && a.wb == b.wb &&
           a.wr == b.wr && a.wq == b.wq && a.wk == b.wk &&
           a.bp == b.bp && a.bn == b.bn && a.bb == b.bb &&
           a.br == b.br && a.bq == b.bq && a.bk == b.bk &&
           a.whiteToMove == b.whiteToMove &&
           a.castlingRights == b.castlingRights &&
           a.enPassantSquare == b.enPassantSquare;
}

long long perft(Board& b, int depth) {
    if (depth == 0) return 1;

    std::vector<Move> moves = generateLegalMoves(b);
    if (depth == 1) return static_cast<long long>(moves.size());

    long long nodes = 0;

    for (const Move& m : moves) {
        Undo u;
        makeMove(b, m, u);
        nodes += perft(b, depth - 1);
        undoMove(b, m, u);
    }

    return nodes;
}

bool require(bool condition, const std::string& name) {
    if (condition) return true;

    std::cerr << "FAIL: " << name << "\n";
    return false;
}

std::set<std::string> moveSet(const std::vector<Move>& moves) {
    std::set<std::string> out;
    for (const Move& m : moves) out.insert(moveToString(m));
    return out;
}

bool testGenerateLegalMovesKeepsSideToMove() {
    Board board;
    parseFEN("4k3/8/8/4p3/4P3/8/8/4K3 w - - 0 1", board);

    Board before = board;
    std::vector<Move> moves = generateLegalMoves(board);

    std::set<std::string> got;
    for (const Move& m : moves) got.insert(moveToString(m));

    const std::set<std::string> expected = {"e1d1", "e1d2", "e1e2", "e1f1", "e1f2"};

    return require(sameBoard(board, before), "generateLegalMoves should restore board state") &&
           require(got == expected, "white-to-move king vs king+pawn position should keep white moves");
}

bool testSearchKeepsBoardState() {
    Board board;
    parseFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);

    Board before = board;
    Move best = findBestMove(board, 2);
    std::string move = moveToString(best);

    return require(sameBoard(board, before), "findBestMove should not mutate the board") &&
           require(move != "none", "findBestMove should return a concrete move on the start position");
}

bool testStartPositionPerft() {
    Board board;
    parseFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);

    const std::array<long long, 4> expected = {20, 400, 8902, 197281};

    for (int depth = 1; depth <= 4; ++depth) {
        Board copy = board;
        long long nodes = perft(copy, depth);
        if (!require(nodes == expected[depth - 1], "start position perft depth " + std::to_string(depth))) {
            return false;
        }
    }

    return true;
}

bool testInvalidMoveStringGuard() {
    Move invalid(64, 56, KING);
    return require(moveToString(invalid) == "none", "invalid squares should not print offboard coordinates");
}

bool testPromotionMoves() {
    Board board;
    parseFEN("6k1/4P3/8/8/8/8/8/4K3 w - - 0 1", board);

    std::set<std::string> got = moveSet(generateLegalMoves(board));
    const std::set<std::string> expected = {"e7e8b", "e7e8n", "e7e8q", "e7e8r"};

    for (const std::string& move : expected) {
        if (!require(got.count(move) == 1, "promotion move " + move + " should be generated")) {
            return false;
        }
    }

    return true;
}

bool testPromotionCaptureMoves() {
    Board board;
    parseFEN("3r2k1/4P3/8/8/8/8/8/4K3 w - - 0 1", board);

    std::set<std::string> got = moveSet(generateLegalMoves(board));
    const std::set<std::string> expected = {"e7d8b", "e7d8n", "e7d8q", "e7d8r"};

    for (const std::string& move : expected) {
        if (!require(got.count(move) == 1, "promotion capture " + move + " should be generated")) {
            return false;
        }
    }

    return true;
}

bool testBlackPromotionMoves() {
    Board board;
    parseFEN("4k3/8/8/8/8/8/4p3/6K1 b - - 0 1", board);

    std::set<std::string> got = moveSet(generateLegalMoves(board));
    const std::set<std::string> expected = {"e2e1b", "e2e1n", "e2e1q", "e2e1r"};

    for (const std::string& move : expected) {
        if (!require(got.count(move) == 1, "black promotion move " + move + " should be generated")) {
            return false;
        }
    }

    return true;
}

bool testEnPassantGenerationAndMakeMove() {
    Board board;
    parseFEN("6k1/8/8/3pP3/8/8/8/4K3 w - d6 0 1", board);

    std::vector<Move> moves = generateLegalMoves(board);
    Move enPassant = Move(-1, -1, PAWN);

    for (const Move& move : moves) {
        if (moveToString(move) == "e5d6") {
            enPassant = move;
            break;
        }
    }

    if (!require(enPassant.flag == ENPASSANT, "en passant move should be generated with ENPASSANT flag")) {
        return false;
    }

    Board before = board;
    Undo undo;
    makeMove(board, enPassant, undo);

    bool movedPawn = board.wp == (1ULL << 43);
    bool capturedPawnRemoved = board.bp == 0;
    bool sideFlipped = !board.whiteToMove;
    bool epCleared = board.enPassantSquare == -1;

    undoMove(board, enPassant, undo);

    return require(movedPawn, "en passant should move the pawn to the capture square") &&
           require(capturedPawnRemoved, "en passant should remove the captured pawn") &&
           require(sideFlipped, "en passant should flip the side to move") &&
           require(epCleared, "en passant should clear the en passant square") &&
           require(sameBoard(board, before), "undo after en passant should restore the board");
}

bool testIllegalEnPassantRejected() {
    Board board;
    parseFEN("4r1k1/8/8/3pP3/8/8/8/4K3 w - d6 0 1", board);

    std::set<std::string> got = moveSet(generateLegalMoves(board));
    return require(got.count("e5d6") == 0, "en passant should be rejected when it exposes the king");
}

bool testBlackEnPassantGenerationAndMakeMove() {
    Board board;
    parseFEN("4k3/8/8/8/3pP3/8/8/6K1 b - e3 0 1", board);

    std::vector<Move> moves = generateLegalMoves(board);
    Move enPassant = Move(-1, -1, PAWN);

    for (const Move& move : moves) {
        if (moveToString(move) == "d4e3") {
            enPassant = move;
            break;
        }
    }

    if (!require(enPassant.flag == ENPASSANT, "black en passant move should be generated with ENPASSANT flag")) {
        return false;
    }

    Board before = board;
    Undo undo;
    makeMove(board, enPassant, undo);

    bool movedPawn = board.bp == (1ULL << 20);
    bool capturedPawnRemoved = board.wp == 0;
    bool sideFlipped = board.whiteToMove;
    bool epCleared = board.enPassantSquare == -1;

    undoMove(board, enPassant, undo);

    return require(movedPawn, "black en passant should move the pawn to the capture square") &&
           require(capturedPawnRemoved, "black en passant should remove the captured pawn") &&
           require(sideFlipped, "black en passant should flip the side to move") &&
           require(epCleared, "black en passant should clear the en passant square") &&
           require(sameBoard(board, before), "undo after black en passant should restore the board");
}

bool testCastlingMoves() {
    Board board;
    parseFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", board);

    std::vector<Move> moves = generateLegalMoves(board);
    std::set<std::string> got = moveSet(moves);

    if (!require(got.count("e1g1") == 1, "white kingside castling should be generated")) return false;
    if (!require(got.count("e1c1") == 1, "white queenside castling should be generated")) return false;

    Move castle = Move(-1, -1, KING);
    for (const Move& move : moves) {
        if (moveToString(move) == "e1g1") {
            castle = move;
            break;
        }
    }

    if (!require(castle.flag == CASTLING, "castling move should use CASTLING flag")) return false;

    Board before = board;
    Undo undo;
    makeMove(board, castle, undo);

    bool kingMoved = board.wk == (1ULL << 6);
    bool rookMoved = board.wr == ((1ULL << 0) | (1ULL << 5));
    bool rightsCleared = (board.castlingRights & 3) == 0;

    undoMove(board, castle, undo);

    return require(kingMoved, "castling should move the king to g1") &&
           require(rookMoved, "castling should move the rook to f1") &&
           require(rightsCleared, "castling should clear white castling rights") &&
           require(sameBoard(board, before), "undo after castling should restore the board");
}

bool testCastlingBlockedByAttack() {
    Board board;
    parseFEN("r3k2r/8/8/8/2b5/8/8/R3K2R w KQkq - 0 1", board);

    std::set<std::string> got = moveSet(generateLegalMoves(board));
    return require(got.count("e1g1") == 0, "castling through an attacked square should be rejected");
}

bool testBlackCastlingMoves() {
    Board board;
    parseFEN("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", board);

    std::vector<Move> moves = generateLegalMoves(board);
    std::set<std::string> got = moveSet(moves);

    if (!require(got.count("e8g8") == 1, "black kingside castling should be generated")) return false;
    if (!require(got.count("e8c8") == 1, "black queenside castling should be generated")) return false;

    Move castle = Move(-1, -1, KING);
    for (const Move& move : moves) {
        if (moveToString(move) == "e8g8") {
            castle = move;
            break;
        }
    }

    if (!require(castle.flag == CASTLING, "black castling move should use CASTLING flag")) return false;

    Board before = board;
    Undo undo;
    makeMove(board, castle, undo);

    bool kingMoved = board.bk == (1ULL << 62);
    bool rookMoved = board.br == ((1ULL << 56) | (1ULL << 61));
    bool rightsCleared = (board.castlingRights & 12) == 0;

    undoMove(board, castle, undo);

    return require(kingMoved, "black castling should move the king to g8") &&
           require(rookMoved, "black castling should move the rook to f8") &&
           require(rightsCleared, "black castling should clear black castling rights") &&
           require(sameBoard(board, before), "undo after black castling should restore the board");
}

bool testKiwipetePerft() {
    Board board;
    parseFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", board);

    const std::array<long long, 3> expected = {48, 2039, 97862};

    for (int depth = 1; depth <= 3; ++depth) {
        Board copy = board;
        long long nodes = perft(copy, depth);
        if (!require(nodes == expected[depth - 1], "kiwipete perft depth " + std::to_string(depth))) {
            return false;
        }
    }

    return true;
}

} // namespace

int main() {
    bool ok = true;

    ok = testGenerateLegalMovesKeepsSideToMove() && ok;
    ok = testSearchKeepsBoardState() && ok;
    ok = testStartPositionPerft() && ok;
    ok = testInvalidMoveStringGuard() && ok;
    ok = testPromotionMoves() && ok;
    ok = testPromotionCaptureMoves() && ok;
    ok = testBlackPromotionMoves() && ok;
    ok = testEnPassantGenerationAndMakeMove() && ok;
    ok = testIllegalEnPassantRejected() && ok;
    ok = testBlackEnPassantGenerationAndMakeMove() && ok;
    ok = testCastlingMoves() && ok;
    ok = testCastlingBlockedByAttack() && ok;
    ok = testBlackCastlingMoves() && ok;
    ok = testKiwipetePerft() && ok;

    if (!ok) return 1;

    std::cout << "All regression tests passed.\n";
    return 0;
}
