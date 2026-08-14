#include "move_to_string.hpp"
#include "../utils/types.hpp"

// Convert square (0–63) → "e2"
std::string squareToString(int sq) {
  if (sq < 0 || sq >= 64) return "??";

  char file = 'a' + (sq % 8);
  char rank = '1' + (sq / 8);
  return std::string() + file + rank;
}

// Convert Move → "e2e4", "e7e8q"
std::string moveToString(const Move& m) {
    if (m.from < 0 || m.from >= 64 || m.to < 0 || m.to >= 64) return "none";

    std::string move = squareToString(m.from) + squareToString(m.to);

    // Promotion
    if (m.promotion != -1) {
        char promo = 'q';

        if (m.promotion == KNIGHT) promo = 'n';
        else if (m.promotion == ROOK) promo = 'r';
        else if (m.promotion == BISHOP) promo = 'b';
        else if (m.promotion == QUEEN) promo = 'q';

        move += promo;
    }

    return move;
}
