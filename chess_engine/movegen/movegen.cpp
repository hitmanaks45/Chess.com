#include "movegen.hpp"
#include "legal.hpp"
#include <cmath>

using namespace std;

// ================= HELPERS =================

inline int popLSB(uint64_t &bb) {
    int sq = __builtin_ctzll(bb);
    bb &= bb - 1;
    return sq;
}

inline uint64_t getWhite(const Board& b) {
    return b.wp | b.wn | b.wb | b.wr | b.wq | b.wk;
}

inline uint64_t getBlack(const Board& b) {
    return b.bp | b.bn | b.bb | b.br | b.bq | b.bk;
}

inline bool isPromotionRank(int rank, bool white) {
    return white ? rank == 6 : rank == 1;
}

void addPromotionMoves(vector<Move>& moves, int from, int to, bool capture) {
    const int promos[4] = {QUEEN, ROOK, BISHOP, KNIGHT};

    for (int promo : promos) {
        moves.emplace_back(from, to, PAWN, capture ? PAWN : -1, promo,
                           capture ? CAPTURE : PROMOTION);
    }
}

// ================= PAWNS =================

void generatePawnMoves(const Board& b, vector<Move>& moves) {
    bool white = b.whiteToMove;

    uint64_t pawns = white ? b.wp : b.bp;
    uint64_t own   = white ? getWhite(b) : getBlack(b);
    uint64_t opp   = white ? getBlack(b) : getWhite(b);
    uint64_t all   = own | opp;

    while (pawns) {
        int sq = popLSB(pawns);
        int rank = sq / 8;

        int dir = white ? 8 : -8;
        int to = sq + dir;

        // single push
        if (to >= 0 && to < 64 && !(all & (1ULL << to))) {
            if (isPromotionRank(rank, white)) {
                addPromotionMoves(moves, sq, to, false);
            } else {
                moves.emplace_back(sq, to, PAWN);

                // double push (WITH BOUNDS CHECK)
                if (white && rank == 1) {
                    int to2 = sq + 16;
                    if (to2 < 64 && !(all & (1ULL << to2)))
                        moves.emplace_back(sq, to2, PAWN);
                }

                if (!white && rank == 6) {
                    int to2 = sq - 16;
                    if (to2 >= 0 && !(all & (1ULL << to2)))
                        moves.emplace_back(sq, to2, PAWN);
                }
            }
        }

        // captures
        int caps[2] = {sq + (white ? 7 : -7), sq + (white ? 9 : -9)};

        for (int c : caps) {
            if (c < 0 || c >= 64) continue;

            if (abs((c % 8) - (sq % 8)) != 1) continue;

            if (opp & (1ULL << c)) {
                if (isPromotionRank(rank, white)) {
                    addPromotionMoves(moves, sq, c, true);
                } else {
                    moves.emplace_back(sq, c, PAWN, PAWN, -1, CAPTURE);
                }
            } else if (c == b.enPassantSquare && (white ? rank == 4 : rank == 3)) {
                moves.emplace_back(sq, c, PAWN, PAWN, -1, ENPASSANT);
            }
        }
    }
}

// ================= KNIGHTS =================

const int knightOffsets[8] = {17,15,10,6,-17,-15,-10,-6};

void generateKnightMoves(const Board& b, vector<Move>& moves) {
    bool white = b.whiteToMove;

    uint64_t knights = white ? b.wn : b.bn;
    uint64_t own = white ? getWhite(b) : getBlack(b);
    uint64_t opp = white ? getBlack(b) : getWhite(b);

    while (knights) {
        int sq = popLSB(knights);

        for (int off : knightOffsets) {
            int to = sq + off;

            if (to < 0 || to >= 64) continue;

            if (abs((to % 8) - (sq % 8)) > 2) continue;

            if (own & (1ULL << to)) continue;

            if (opp & (1ULL << to))
                moves.emplace_back(sq, to, KNIGHT, KNIGHT, -1, CAPTURE);
            else
                moves.emplace_back(sq, to, KNIGHT);
        }
    }
}

// ================= KING =================

const int kingOffsets[8] = {8,-8,1,-1,9,-9,7,-7};

void generateCastlingMoves(const Board& b, vector<Move>& moves) {
    bool white = b.whiteToMove;
    uint64_t all = getWhite(b) | getBlack(b);

    if (white) {
        if (b.wk == (1ULL << 4)) {
            if ((b.castlingRights & 1) &&
                (b.wr & (1ULL << 7)) &&
                !(all & (1ULL << 5)) &&
                !(all & (1ULL << 6)) &&
                !isSquareAttacked(b, 4, false) &&
                !isSquareAttacked(b, 5, false) &&
                !isSquareAttacked(b, 6, false)) {
                moves.emplace_back(4, 6, KING, -1, -1, CASTLING);
            }

            if ((b.castlingRights & 2) &&
                (b.wr & (1ULL << 0)) &&
                !(all & (1ULL << 1)) &&
                !(all & (1ULL << 2)) &&
                !(all & (1ULL << 3)) &&
                !isSquareAttacked(b, 4, false) &&
                !isSquareAttacked(b, 3, false) &&
                !isSquareAttacked(b, 2, false)) {
                moves.emplace_back(4, 2, KING, -1, -1, CASTLING);
            }
        }
    } else {
        if (b.bk == (1ULL << 60)) {
            if ((b.castlingRights & 4) &&
                (b.br & (1ULL << 63)) &&
                !(all & (1ULL << 61)) &&
                !(all & (1ULL << 62)) &&
                !isSquareAttacked(b, 60, true) &&
                !isSquareAttacked(b, 61, true) &&
                !isSquareAttacked(b, 62, true)) {
                moves.emplace_back(60, 62, KING, -1, -1, CASTLING);
            }

            if ((b.castlingRights & 8) &&
                (b.br & (1ULL << 56)) &&
                !(all & (1ULL << 57)) &&
                !(all & (1ULL << 58)) &&
                !(all & (1ULL << 59)) &&
                !isSquareAttacked(b, 60, true) &&
                !isSquareAttacked(b, 59, true) &&
                !isSquareAttacked(b, 58, true)) {
                moves.emplace_back(60, 58, KING, -1, -1, CASTLING);
            }
        }
    }
}

void generateKingMoves(const Board& b, vector<Move>& moves) {
    bool white = b.whiteToMove;

    uint64_t king = white ? b.wk : b.bk;
    if (king == 0) return;

    int sq = __builtin_ctzll(king);

    uint64_t own = white ? getWhite(b) : getBlack(b);
    uint64_t opp = white ? getBlack(b) : getWhite(b);

    for (int off : kingOffsets) {
        int to = sq + off;

        if (to < 0 || to >= 64) continue;

        if (abs((to % 8) - (sq % 8)) > 1) continue;

        if (own & (1ULL << to)) continue;

        if (opp & (1ULL << to))
            moves.emplace_back(sq, to, KING, KING, -1, CAPTURE);
        else
            moves.emplace_back(sq, to, KING);
    }

    generateCastlingMoves(b, moves);
}

// ================= SLIDING =================

void generateSliding(const Board& b, vector<Move>& moves,
                     uint64_t pieces, const int dirs[], int dirCount, int pieceType) {

    bool white = b.whiteToMove;

    uint64_t own = white ? getWhite(b) : getBlack(b);
    uint64_t opp = white ? getBlack(b) : getWhite(b);

    while (pieces) {
        int sq = popLSB(pieces);

        for (int i = 0; i < dirCount; i++) {
            int d = dirs[i];
            int to = sq;

            while (true) {
                int prev = to;
                to += d;

                if (to < 0 || to >= 64) break;

                int fileDiff = abs((to % 8) - (prev % 8));

                // horizontal wrap
                if ((d == 1 || d == -1) && fileDiff != 1) break;

                // diagonal wrap
                if ((d == 9 || d == -9 || d == 7 || d == -7) && fileDiff != 1) break;

                if (own & (1ULL << to)) break;

                if (opp & (1ULL << to)) {
                    moves.emplace_back(sq, to, pieceType, pieceType, -1, CAPTURE);
                    break;
                }

                moves.emplace_back(sq, to, pieceType);
            }
        }
    }
}

// ================= MAIN =================

vector<Move> generateMoves(const Board& b) {
    vector<Move> moves;

    generatePawnMoves(b, moves);
    generateKnightMoves(b, moves);

    int bishopDirs[4] = {9,7,-9,-7};
    int rookDirs[4]   = {8,-8,1,-1};
    int queenDirs[8]  = {8,-8,1,-1,9,7,-9,-7};

    generateSliding(b, moves, b.whiteToMove ? b.wb : b.bb, bishopDirs, 4, BISHOP);
    generateSliding(b, moves, b.whiteToMove ? b.wr : b.br, rookDirs, 4, ROOK);
    generateSliding(b, moves, b.whiteToMove ? b.wq : b.bq, queenDirs, 8, QUEEN);

    generateKingMoves(b, moves);

    return moves;
}
