#include "makemove.hpp"
#include <cstdlib>

// Helper: remove piece from square
inline void clearBit(uint64_t &bb, int sq) {
  bb &= ~(1ULL << sq);
}

// Helper: set piece
inline void setBit(uint64_t &bb, int sq) {
  bb |= (1ULL << sq);
}

inline void clearSquare(Board& b, int sq) {
    clearBit(b.wp, sq); clearBit(b.wn, sq); clearBit(b.wb, sq);
    clearBit(b.wr, sq); clearBit(b.wq, sq); clearBit(b.wk, sq);

    clearBit(b.bp, sq); clearBit(b.bn, sq); clearBit(b.bb, sq);
    clearBit(b.br, sq); clearBit(b.bq, sq); clearBit(b.bk, sq);
}

inline void setPiece(Board& b, bool white, int piece, int sq) {
    if (white) {
        if (piece == PAWN) setBit(b.wp, sq);
        else if (piece == KNIGHT) setBit(b.wn, sq);
        else if (piece == BISHOP) setBit(b.wb, sq);
        else if (piece == ROOK) setBit(b.wr, sq);
        else if (piece == QUEEN) setBit(b.wq, sq);
        else if (piece == KING) setBit(b.wk, sq);
    } else {
        if (piece == PAWN) setBit(b.bp, sq);
        else if (piece == KNIGHT) setBit(b.bn, sq);
        else if (piece == BISHOP) setBit(b.bb, sq);
        else if (piece == ROOK) setBit(b.br, sq);
        else if (piece == QUEEN) setBit(b.bq, sq);
        else if (piece == KING) setBit(b.bk, sq);
    }
}

void updateCastlingRights(Board& b, const Move& m, bool white, int captureSq) {
    if (white) {
        if (m.piece == KING) b.castlingRights &= ~(1 | 2);

        if (m.piece == ROOK) {
            if (m.from == 0) b.castlingRights &= ~2;
            if (m.from == 7) b.castlingRights &= ~1;
        }

        if (captureSq == 56) b.castlingRights &= ~8;
        if (captureSq == 63) b.castlingRights &= ~4;
    } else {
        if (m.piece == KING) b.castlingRights &= ~(4 | 8);

        if (m.piece == ROOK) {
            if (m.from == 56) b.castlingRights &= ~8;
            if (m.from == 63) b.castlingRights &= ~4;
        }

        if (captureSq == 0) b.castlingRights &= ~2;
        if (captureSq == 7) b.castlingRights &= ~1;
    }
}

// Save full board state
void saveState(const Board& b, Undo& u) {
  u.whiteToMove = b.whiteToMove;
  u.castlingRights = b.castlingRights;
  u.enPassantSquare = b.enPassantSquare;

  u.wp = b.wp; u.wn = b.wn; u.wb = b.wb;
  u.wr = b.wr; u.wq = b.wq; u.wk = b.wk;

  u.bp = b.bp; u.bn = b.bn; u.bb = b.bb;
  u.br = b.br; u.bq = b.bq; u.bk = b.bk;
}

// Restore state
void restoreState(Board& b, const Undo& u) {
    b.whiteToMove = u.whiteToMove;
    b.castlingRights = u.castlingRights;
    b.enPassantSquare = u.enPassantSquare;

    b.wp = u.wp; b.wn = u.wn; b.wb = u.wb;
    b.wr = u.wr; b.wq = u.wq; b.wk = u.wk;

    b.bp = u.bp; b.bn = u.bn; b.bb = u.bb;
    b.br = u.br; b.bq = u.bq; b.bk = u.bk;
}

void makeMove(Board& b, const Move& m, Undo& u) {
    saveState(b, u);

    bool white = b.whiteToMove;
    uint64_t fromMask = (1ULL << m.from);
    int captureSq = -1;

    if (m.flag == CAPTURE) {
        captureSq = m.to;
    } else if (m.flag == ENPASSANT) {
        captureSq = white ? (m.to - 8) : (m.to + 8);
    }

    updateCastlingRights(b, m, white, captureSq);

    // Remove piece from source
    if (white) {
        if (m.piece == 0) b.wp &= ~fromMask;
        else if (m.piece == 1) b.wn &= ~fromMask;
        else if (m.piece == 2) b.wb &= ~fromMask;
        else if (m.piece == 3) b.wr &= ~fromMask;
        else if (m.piece == 4) b.wq &= ~fromMask;
        else if (m.piece == 5) b.wk &= ~fromMask;
    } else {
        if (m.piece == 0) b.bp &= ~fromMask;
        else if (m.piece == 1) b.bn &= ~fromMask;
        else if (m.piece == 2) b.bb &= ~fromMask;
        else if (m.piece == 3) b.br &= ~fromMask;
        else if (m.piece == 4) b.bq &= ~fromMask;
        else if (m.piece == 5) b.bk &= ~fromMask;
    }

    if (captureSq != -1) {
        clearSquare(b, captureSq);
    }

    if (m.flag == CASTLING) {
        setPiece(b, white, KING, m.to);

        if (white) {
            if (m.to == 6) {
                clearBit(b.wr, 7);
                setBit(b.wr, 5);
            } else if (m.to == 2) {
                clearBit(b.wr, 0);
                setBit(b.wr, 3);
            }
        } else {
            if (m.to == 62) {
                clearBit(b.br, 63);
                setBit(b.br, 61);
            } else if (m.to == 58) {
                clearBit(b.br, 56);
                setBit(b.br, 59);
            }
        }
    } else if (m.promotion != -1) {
        setPiece(b, white, m.promotion, m.to);
    } else {
        setPiece(b, white, m.piece, m.to);
    }

    b.enPassantSquare = -1;

    if (m.piece == PAWN && std::abs(m.to - m.from) == 16) {
        b.enPassantSquare = (m.from + m.to) / 2;
    }

    // Switch side
    b.whiteToMove = !b.whiteToMove;
}

void undoMove(Board& b, const Move&, const Undo& u) {
    restoreState(b, u);
}
