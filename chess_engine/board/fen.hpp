#ifndef FEN_HPP
#define FEN_HPP

#include <string>
#include "board.hpp"

void parseFEN(const std::string& fen, Board& board);

#endif