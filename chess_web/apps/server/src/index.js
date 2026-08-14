import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import dotenv from 'dotenv';
import express from 'express';
import { Chess } from 'chess.js';

import { EngineService } from './engine/EngineService.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceRoot = path.resolve(__dirname, '..', '..', '..');
const webDistPath = path.resolve(workspaceRoot, 'apps', 'web', 'dist');

dotenv.config({ path: path.resolve(workspaceRoot, '.env') });

function parseEngineArgs(rawValue) {
  if (!rawValue) {
    return [];
  }

  try {
    const parsed = JSON.parse(rawValue);
    return Array.isArray(parsed) ? parsed.map(String) : [];
  } catch {
    return rawValue
      .split(' ')
      .map((value) => value.trim())
      .filter(Boolean);
  }
}

function applyCoordinateMove(game, moveText) {
  const normalized = moveText.trim().toLowerCase();
  const moveShape = {
    from: normalized.slice(0, 2),
    to: normalized.slice(2, 4)
  };

  if (normalized.length === 5) {
    moveShape.promotion = normalized[4];
  }

  let move = game.move(moveShape);

  if (!move) {
    const movingPiece = game.get(moveShape.from);
    const promotionRank = moveShape.to[1];
    const needsPromotion =
      movingPiece?.type === 'p' && (promotionRank === '1' || promotionRank === '8');

    if (needsPromotion && !moveShape.promotion) {
      move = game.move({ ...moveShape, promotion: 'q' });
    }
  }

  if (!move) {
    throw new Error(`Engine returned an illegal move: ${moveText}`);
  }

  return move;
}

function evaluateResult(game, playerColor, loserOnTime = null) {
  if (loserOnTime) {
    return {
      title: loserOnTime === playerColor ? 'You lose' : 'You win',
      detail:
        loserOnTime === playerColor
          ? 'Your clock hit zero.'
          : 'The engine ran out of time.'
    };
  }

  if (game.isCheckmate()) {
    const sideToMove = game.turn();
    return {
      title: sideToMove === playerColor ? 'You lose' : 'You win',
      detail: sideToMove === playerColor ? 'Checkmate.' : 'Checkmate. Nice finish.'
    };
  }

  if (game.isStalemate()) {
    return { title: 'Draw', detail: 'Stalemate.' };
  }

  if (game.isThreefoldRepetition()) {
    return { title: 'Draw', detail: 'Threefold repetition.' };
  }

  if (game.isInsufficientMaterial()) {
    return { title: 'Draw', detail: 'Insufficient material.' };
  }

  if (game.isDraw()) {
    return { title: 'Draw', detail: 'The game ended in a draw.' };
  }

  return null;
}

function openBrowser(url) {
  if (process.platform === 'win32') {
    spawn('cmd', ['/c', 'start', '', url], {
      detached: true,
      stdio: 'ignore'
    }).unref();
    return;
  }

  if (process.platform === 'darwin') {
    spawn('open', [url], { detached: true, stdio: 'ignore' }).unref();
    return;
  }

  spawn('xdg-open', [url], { detached: true, stdio: 'ignore' }).unref();
}

const config = {
  port: Number(process.env.PORT || 3000),
  enginePath: process.env.ENGINE_PATH || '',
  engineWorkdir: process.env.ENGINE_WORKDIR || '',
  engineArgs: parseEngineArgs(process.env.ENGINE_ARGS || '[]'),
  moveTimeoutMs: Number(process.env.ENGINE_MOVE_TIMEOUT_MS || 15000),
  autoOpenBrowser: String(process.env.AUTO_OPEN_BROWSER || 'false') === 'true'
};

const engineService = new EngineService(config);
const app = express();

app.use(express.json({ limit: '1mb' }));

app.get('/api/health', async (_request, response) => {
  response.json({
    ok: true,
    engine: engineService.health()
  });
});

app.post('/api/engine/move', async (request, response) => {
  const { fen, playerColor } = request.body ?? {};

  if (!fen || typeof fen !== 'string') {
    response.status(400).json({ error: 'Request body must include a FEN string.' });
    return;
  }

  let game;
  try {
    game = new Chess(fen);
  } catch {
    response.status(400).json({ error: 'Invalid FEN provided.' });
    return;
  }

  try {
    const rawMove = await engineService.requestBestMove(fen);
    const move = applyCoordinateMove(game, rawMove);
    const result = evaluateResult(game, playerColor || null);

    response.json({
      move: rawMove,
      san: move.san,
      fen: game.fen(),
      turn: game.turn(),
      isCheck: game.isCheck(),
      isGameOver: game.isGameOver(),
      result
    });
  } catch (error) {
    response.status(500).json({
      error: error.message || 'Engine request failed.'
    });
  }
});

if (fs.existsSync(webDistPath)) {
  app.use(express.static(webDistPath));

  app.get(/^(?!\/api).*/, (_request, response) => {
    response.sendFile(path.join(webDistPath, 'index.html'));
  });
}

app.listen(config.port, () => {
  const url = `http://localhost:${config.port}`;
  console.log(`Chess server running on ${url}`);

  if (config.autoOpenBrowser && fs.existsSync(webDistPath)) {
    openBrowser(url);
  }
});

function shutdown() {
  engineService.stop();
  process.exit(0);
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
