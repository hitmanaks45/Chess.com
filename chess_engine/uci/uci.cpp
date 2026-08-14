#include "uci.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include "../board/board.hpp"
#include "../board/fen.hpp"
#include "../move/makemove.hpp"
#include "../movegen/legal.hpp"
#include "../search/search.hpp"
#include "../utils/move_to_string.hpp"

namespace {

constexpr const char* kEngineName = "DebaChess";
constexpr const char* kEngineAuthor = "Debashis Baral";
constexpr const char* kStartposFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int kMinDepth = 1;
constexpr int kMaxDepth = 64;
constexpr int kDefaultDepth = 4;
constexpr int kDefaultMoveOverheadMs = 25;
constexpr int kFallbackThinkMs = 1000;
constexpr int kMinThinkMs = 50;

struct UciOptions {
    int defaultDepth = kDefaultDepth;
    int moveOverheadMs = kDefaultMoveOverheadMs;
};

struct GoParams {
    int depth = -1;
    int movetime = -1;
    int wtime = -1;
    int btime = -1;
    int winc = 0;
    int binc = 0;
    int movestogo = -1;
    bool infinite = false;
};

std::string trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

int parseInt(const std::string& text, int fallback) {
    std::stringstream ss(text);
    int value = fallback;
    ss >> value;
    return ss.fail() ? fallback : value;
}

std::vector<std::string> splitWords(const std::string& line) {
    std::stringstream ss(line);
    std::vector<std::string> words;
    std::string word;

    while (ss >> word) words.push_back(word);

    return words;
}

Board makeStartposBoard() {
    Board board;
    parseFEN(kStartposFen, board);
    return board;
}

std::string uciBestMoveString(const Move& move) {
    std::string text = moveToString(move);
    return text == "none" ? "0000" : text;
}

Move findMoveFromText(Board& board, const std::string& moveText) {
    std::vector<Move> legalMoves = generateLegalMoves(board);

    for (const Move& move : legalMoves) {
        if (moveToString(move) == moveText) return move;
    }

    return Move(-1, -1, KING);
}

bool applyMoveText(Board& board, const std::string& moveText) {
    Move move = findMoveFromText(board, moveText);
    if (move.from == -1) return false;

    Undo undo;
    makeMove(board, move, undo);
    return true;
}

void applyMoveList(Board& board, const std::string& movesText, std::ostream& out) {
    std::vector<std::string> moves = splitWords(movesText);

    for (const std::string& moveText : moves) {
        if (!applyMoveText(board, moveText)) {
            out << "info string ignored illegal move " << moveText << "\n";
            break;
        }
    }
}

void handlePosition(Board& board, const std::string& command, std::ostream& out) {
    std::string payload = trim(command.substr(8));

    if (startsWith(payload, "startpos")) {
        board = makeStartposBoard();

        size_t movesPos = payload.find(" moves ");
        if (movesPos != std::string::npos) {
            applyMoveList(board, payload.substr(movesPos + 7), out);
        }

        return;
    }

    if (startsWith(payload, "fen ")) {
        std::string fenPayload = payload.substr(4);
        size_t movesPos = fenPayload.find(" moves ");
        std::string fen = movesPos == std::string::npos
            ? fenPayload
            : fenPayload.substr(0, movesPos);

        parseFEN(trim(fen), board);

        if (movesPos != std::string::npos) {
            applyMoveList(board, fenPayload.substr(movesPos + 7), out);
        }
    }
}

void handleSetOption(const std::string& command, UciOptions& options) {
    std::vector<std::string> words = splitWords(command);
    std::string name;
    std::string value;
    bool readingName = false;
    bool readingValue = false;

    for (size_t i = 1; i < words.size(); ++i) {
        if (words[i] == "name") {
            readingName = true;
            readingValue = false;
            continue;
        }

        if (words[i] == "value") {
            readingName = false;
            readingValue = true;
            continue;
        }

        if (readingName) {
            if (!name.empty()) name += " ";
            name += words[i];
        } else if (readingValue) {
            if (!value.empty()) value += " ";
            value += words[i];
        }
    }

    std::string key = toLower(name);

    if (key == "defaultdepth") {
        options.defaultDepth = clampInt(parseInt(value, options.defaultDepth), kMinDepth, kMaxDepth);
    } else if (key == "moveoverhead") {
        options.moveOverheadMs = clampInt(parseInt(value, options.moveOverheadMs), 0, 5000);
    }
}

GoParams parseGoParams(const std::string& command) {
    std::stringstream ss(command);
    GoParams params;
    std::string token;

    ss >> token; // "go"

    while (ss >> token) {
        if (token == "depth") ss >> params.depth;
        else if (token == "movetime") ss >> params.movetime;
        else if (token == "wtime") ss >> params.wtime;
        else if (token == "btime") ss >> params.btime;
        else if (token == "winc") ss >> params.winc;
        else if (token == "binc") ss >> params.binc;
        else if (token == "movestogo") ss >> params.movestogo;
        else if (token == "infinite") params.infinite = true;
    }

    return params;
}

int computeTimeBudgetMs(const GoParams& params, bool whiteToMove, int moveOverheadMs) {
    if (params.movetime > 0) {
        return std::max(1, params.movetime - moveOverheadMs);
    }

    int timeLeft = whiteToMove ? params.wtime : params.btime;
    int increment = whiteToMove ? params.winc : params.binc;

    if (timeLeft <= 0) return kFallbackThinkMs;

    int movesToGo = params.movestogo > 0 ? params.movestogo : 30;
    int budget = timeLeft / std::max(1, movesToGo);
    budget += (3 * increment) / 4;
    budget -= moveOverheadMs;

    budget = std::max(kMinThinkMs, budget);
    budget = std::min(budget, std::max(kMinThinkMs, timeLeft / 2));

    return budget;
}

void emitInfo(std::ostream& out, const SearchResult& result, long long elapsedMs) {
    out << "info depth " << result.depth
        << " score cp " << result.eval
        << " time " << elapsedMs;

    std::string best = uciBestMoveString(result.bestMove);
    if (best != "0000") {
        out << " pv " << best;
    }

    out << "\n";
}

SearchResult searchForUci(Board board, const GoParams& params,
                          const UciOptions& options, std::ostream& out) {
    int depthLimit = params.depth > 0
        ? clampInt(params.depth, kMinDepth, kMaxDepth)
        : kMaxDepth;
    int timeBudgetMs = params.depth > 0
        ? -1
        : computeTimeBudgetMs(params, board.whiteToMove, options.moveOverheadMs);

    if (params.infinite && params.depth <= 0 && params.movetime <= 0 &&
        params.wtime <= 0 && params.btime <= 0) {
        depthLimit = options.defaultDepth;
        timeBudgetMs = -1;
    } else if (params.depth <= 0 && params.movetime <= 0 &&
               params.wtime <= 0 && params.btime <= 0) {
        depthLimit = options.defaultDepth;
        timeBudgetMs = -1;
    }

    SearchResult best;
    auto start = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= depthLimit; ++depth) {
        SearchResult current = searchBestMove(board, depth);
        auto now = std::chrono::steady_clock::now();
        long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

        if (current.bestMove.from != -1 || depth == 1) {
            best = current;
        }

        emitInfo(out, current, elapsedMs);

        if (depth == depthLimit) break;

        if (timeBudgetMs > 0) {
            if (elapsedMs >= timeBudgetMs) break;

            long long predictedNext = elapsedMs == 0 ? 1 : elapsedMs * 4;
            if (elapsedMs + predictedNext >= timeBudgetMs) break;
        }
    }

    return best;
}

} // namespace

void runUciLoop(std::istream& in, std::ostream& out) {
    Board board = makeStartposBoard();
    UciOptions options;
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line == "uci") {
            out << "id name " << kEngineName << "\n";
            out << "id author " << kEngineAuthor << "\n";
            out << "option name DefaultDepth type spin default " << options.defaultDepth
                << " min 1 max 64\n";
            out << "option name MoveOverhead type spin default " << options.moveOverheadMs
                << " min 0 max 5000\n";
            out << "uciok\n";
        } else if (line == "isready") {
            out << "readyok\n";
        } else if (line == "ucinewgame") {
            board = makeStartposBoard();
        } else if (startsWith(line, "setoption ")) {
            handleSetOption(line, options);
        } else if (startsWith(line, "position ")) {
            handlePosition(board, line, out);
        } else if (startsWith(line, "go")) {
            GoParams params = parseGoParams(line);
            SearchResult result = searchForUci(board, params, options, out);
            out << "bestmove " << uciBestMoveString(result.bestMove) << "\n";
        } else if (line == "stop" || line == "ponderhit" || startsWith(line, "debug ")) {
            // Search is synchronous right now, so these are acknowledged as no-ops.
        } else if (line == "quit") {
            break;
        }

        out.flush();
    }
}
