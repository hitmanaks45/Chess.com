#include "fen.hpp"
#include <sstream>
#include <cctype>

// Map piece to bitboard
void setPiece(Board& b, char piece, int square) {
  uint64_t bit = (1ULL << square);

  switch(piece) {
      case 'P': b.wp |= bit; break;
      case 'N': b.wn |= bit; break;
      case 'B': b.wb |= bit; break;
      case 'R': b.wr |= bit; break;
      case 'Q': b.wq |= bit; break;
      case 'K': b.wk |= bit; break;

      case 'p': b.bp |= bit; break;
      case 'n': b.bn |= bit; break;
      case 'b': b.bb |= bit; break;
      case 'r': b.br |= bit; break;
      case 'q': b.bq |= bit; break;
      case 'k': b.bk |= bit; break;
  }
}

void parseFEN(const std::string& fen, Board& board) {
    std::stringstream ss(fen);

    std::string boardPart, turn, castling, enpassant;
    ss >> boardPart >> turn >> castling >> enpassant;

    // Reset board
    board = Board();

    int square = 56; // start at a8

    for(char c : boardPart) {
        if(c == '/') {
            square -= 16; // move to next rank
        }
        else if(isdigit(c)) {
            square += (c - '0');
        }
        else {
            setPiece(board, c, square);
            square++;
        }
    }

    // Side to move
    board.whiteToMove = (turn == "w");

    // Castling rights
    board.castlingRights = 0;
    for(char c : castling) {
        if(c == 'K') board.castlingRights |= 1;
        if(c == 'Q') board.castlingRights |= 2;
        if(c == 'k') board.castlingRights |= 4;
        if(c == 'q') board.castlingRights |= 8;
    }

    // En passant
    if(enpassant != "-") {
        int file = enpassant[0] - 'a';
        int rank = enpassant[1] - '1';
        board.enPassantSquare = rank * 8 + file;
    } else {
        board.enPassantSquare = -1;
    }
}