#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../uci/uci.hpp"

namespace {

bool require(bool condition, const std::string& name) {
    if (condition) return true;

    std::cerr << "FAIL: " << name << "\n";
    return false;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::string extractBestMove(const std::string& output) {
    std::stringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.rfind("bestmove ", 0) == 0) {
            return line.substr(9);
        }
    }

    return "";
}

bool isPromotionMove(const std::string& move) {
    const std::vector<std::string> promotions = {"e7e8q", "e7e8r", "e7e8b", "e7e8n"};

    for (const std::string& candidate : promotions) {
        if (move == candidate) return true;
    }

    return false;
}

bool testHandshake() {
    std::istringstream input("uci\nisready\nquit\n");
    std::ostringstream output;

    runUciLoop(input, output);

    std::string text = output.str();
    return require(contains(text, "id name"), "uci should print engine name") &&
           require(contains(text, "id author"), "uci should print engine author") &&
           require(contains(text, "option name DefaultDepth"), "uci should expose DefaultDepth option") &&
           require(contains(text, "uciok"), "uci should acknowledge with uciok") &&
           require(contains(text, "readyok"), "isready should respond with readyok");
}

bool testPositionStartposAndGoDepth() {
    std::istringstream input(
        "position startpos moves e2e4 e7e5 g1f3\n"
        "go depth 2\n"
        "quit\n"
    );
    std::ostringstream output;

    runUciLoop(input, output);

    std::string text = output.str();
    std::string bestMove = extractBestMove(text);

    return require(contains(text, "info depth 1"), "go depth should emit depth 1 info") &&
           require(contains(text, "info depth 2"), "go depth should emit depth 2 info") &&
           require(!bestMove.empty(), "go depth should return a bestmove line") &&
           require(bestMove != "0000", "go depth should return a legal move from startpos sequence");
}

bool testFenPromotionSearch() {
    std::istringstream input(
        "position fen 6k1/4P3/8/8/8/8/8/4K3 w - - 0 1\n"
        "go depth 1\n"
        "quit\n"
    );
    std::ostringstream output;

    runUciLoop(input, output);

    return require(isPromotionMove(extractBestMove(output.str())),
                   "UCI search should return a promotion move on a promotion position");
}

bool testMovetimeAndOptions() {
    std::istringstream input(
        "setoption name DefaultDepth value 3\n"
        "setoption name MoveOverhead value 10\n"
        "position startpos\n"
        "go movetime 50\n"
        "quit\n"
    );
    std::ostringstream output;

    runUciLoop(input, output);

    std::string bestMove = extractBestMove(output.str());
    return require(!bestMove.empty(), "go movetime should return a bestmove line") &&
           require(bestMove != "0000", "go movetime should return a legal move");
}

} // namespace

int main() {
    bool ok = true;

    ok = testHandshake() && ok;
    ok = testPositionStartposAndGoDepth() && ok;
    ok = testFenPromotionSearch() && ok;
    ok = testMovetimeAndOptions() && ok;

    if (!ok) return 1;

    std::cout << "All UCI regression tests passed.\n";
    return 0;
}
