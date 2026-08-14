import { useEffect, useRef, useState } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';

const INITIAL_TIME_MS = 10 * 60 * 1000;

function createSoundPlayer() {
  const AudioContextRef = window.AudioContext || window.webkitAudioContext;

  if (!AudioContextRef) {
    return {
      start() {},
      move() {},
      capture() {},
      check() {},
      win() {},
      lose() {}
    };
  }

  let audioContext = null;

  async function ensureContext() {
    if (!audioContext) {
      audioContext = new AudioContextRef();
    }

    if (audioContext.state === 'suspended') {
      await audioContext.resume();
    }

    return audioContext;
  }

  async function pulse({
    frequency,
    duration = 0.12,
    type = 'triangle',
    gain = 0.05,
    offset = 0
  }) {
    const ctx = await ensureContext();
    const startAt = ctx.currentTime + offset;
    const oscillator = ctx.createOscillator();
    const amplifier = ctx.createGain();

    oscillator.type = type;
    oscillator.frequency.setValueAtTime(frequency, startAt);
    amplifier.gain.setValueAtTime(0.0001, startAt);
    amplifier.gain.exponentialRampToValueAtTime(gain, startAt + 0.02);
    amplifier.gain.exponentialRampToValueAtTime(0.0001, startAt + duration);

    oscillator.connect(amplifier);
    amplifier.connect(ctx.destination);
    oscillator.start(startAt);
    oscillator.stop(startAt + duration + 0.02);
  }

  return {
    start() {
      pulse({ frequency: 523, duration: 0.08, gain: 0.04 });
      pulse({ frequency: 659, duration: 0.08, gain: 0.04, offset: 0.08 });
    },
    move() {
      pulse({ frequency: 430, duration: 0.07, gain: 0.03 });
      pulse({ frequency: 520, duration: 0.05, gain: 0.025, offset: 0.07 });
    },
    capture() {
      pulse({ frequency: 250, duration: 0.08, gain: 0.04, type: 'sawtooth' });
      pulse({ frequency: 180, duration: 0.1, gain: 0.035, type: 'square', offset: 0.05 });
    },
    check() {
      pulse({ frequency: 700, duration: 0.1, gain: 0.04 });
      pulse({ frequency: 880, duration: 0.12, gain: 0.045, offset: 0.07 });
    },
    win() {
      pulse({ frequency: 523, duration: 0.12, gain: 0.04 });
      pulse({ frequency: 659, duration: 0.12, gain: 0.04, offset: 0.1 });
      pulse({ frequency: 784, duration: 0.16, gain: 0.05, offset: 0.22 });
    },
    lose() {
      pulse({ frequency: 330, duration: 0.12, gain: 0.04, type: 'sawtooth' });
      pulse({ frequency: 262, duration: 0.14, gain: 0.04, type: 'square', offset: 0.1 });
      pulse({ frequency: 196, duration: 0.18, gain: 0.05, offset: 0.24 });
    }
  };
}

function formatClock(timeMs) {
  const totalSeconds = Math.max(0, Math.ceil(timeMs / 1000));
  const minutes = String(Math.floor(totalSeconds / 60)).padStart(2, '0');
  const seconds = String(totalSeconds % 60).padStart(2, '0');
  return `${minutes}:${seconds}`;
}

function evaluatePosition(game, playerColor, loserOnTime = null) {
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
      detail: sideToMove === playerColor ? 'Checkmate.' : 'Checkmate. Well played.'
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

function buildStatusLine(game, playerColor, engineBusy, result) {
  if (!playerColor) {
    return 'Choose your side to begin.';
  }

  if (result) {
    return `${result.title}. ${result.detail}`;
  }

  if (engineBusy) {
    return 'Engine is thinking...';
  }

  if (game.turn() === playerColor) {
    return game.isCheck() ? 'Your king is in check.' : 'Your move.';
  }

  return game.isCheck() ? 'Engine is in check and must respond.' : 'Engine turn.';
}

export default function App() {
  const [fen, setFen] = useState(() => new Chess().fen());
  const [playerColor, setPlayerColor] = useState(null);
  const [selectedSquare, setSelectedSquare] = useState(null);
  const [legalTargets, setLegalTargets] = useState([]);
  const [engineBusy, setEngineBusy] = useState(false);
  const [result, setResult] = useState(null);
  const [timers, setTimers] = useState({ w: INITIAL_TIME_MS, b: INITIAL_TIME_MS });
  const [activeClock, setActiveClock] = useState(null);
  const [health, setHealth] = useState({ loading: true, online: false, engine: null });
  const [showSidePicker, setShowSidePicker] = useState(true);
  const [boardPixels, setBoardPixels] = useState(0);

  const soundPlayerRef = useRef(null);
  const boardStageRef = useRef(null);

  if (!soundPlayerRef.current) {
    soundPlayerRef.current = createSoundPlayer();
  }

  const game = new Chess(fen);
  const engineReady = health.online && health.engine?.configured;
  const statusLine = buildStatusLine(game, playerColor, engineBusy, result);

  function playResultSound(nextResult) {
    if (!nextResult) {
      return;
    }

    if (nextResult.title === 'You win') {
      soundPlayerRef.current.win();
      return;
    }

    if (nextResult.title === 'You lose') {
      soundPlayerRef.current.lose();
    }
  }

  async function checkHealth() {
    try {
      const response = await fetch('/api/health');
      const payload = await response.json();

      setHealth({
        loading: false,
        online: true,
        engine: payload.engine
      });
    } catch {
      setHealth({
        loading: false,
        online: false,
        engine: null
      });
    }
  }

  useEffect(() => {
    checkHealth();
  }, []);

  useEffect(() => {
    const boardStage = boardStageRef.current;
    if (!boardStage || typeof ResizeObserver === 'undefined') {
      return undefined;
    }

    const updateBoardSize = () => {
      const { width, height } = boardStage.getBoundingClientRect();
      const nextPixels = Math.floor(Math.max(0, Math.min(width - 8, height - 8)));
      setBoardPixels((currentPixels) =>
        currentPixels === nextPixels ? currentPixels : nextPixels
      );
    };

    updateBoardSize();

    const observer = new ResizeObserver(() => {
      updateBoardSize();
    });

    observer.observe(boardStage);
    window.addEventListener('resize', updateBoardSize);

    return () => {
      observer.disconnect();
      window.removeEventListener('resize', updateBoardSize);
    };
  }, []);

  useEffect(() => {
    if (!playerColor || !activeClock || result || showSidePicker) {
      return undefined;
    }

    let lastTick = Date.now();
    const intervalId = window.setInterval(() => {
      const now = Date.now();
      const elapsed = now - lastTick;
      lastTick = now;

      setTimers((currentTimers) => ({
        ...currentTimers,
        [activeClock]: Math.max(0, currentTimers[activeClock] - elapsed)
      }));
    }, 100);

    return () => window.clearInterval(intervalId);
  }, [activeClock, playerColor, result, showSidePicker]);

  useEffect(() => {
    if (!playerColor || result || showSidePicker) {
      return;
    }

    if (timers.w <= 0) {
      const timeoutResult = evaluatePosition(new Chess(fen), playerColor, 'w');
      setResult(timeoutResult);
      setActiveClock(null);
      playResultSound(timeoutResult);
      return;
    }

    if (timers.b <= 0) {
      const timeoutResult = evaluatePosition(new Chess(fen), playerColor, 'b');
      setResult(timeoutResult);
      setActiveClock(null);
      playResultSound(timeoutResult);
    }
  }, [fen, playerColor, result, showSidePicker, timers]);

  useEffect(() => {
    if (!playerColor || result || engineBusy || showSidePicker) {
      return;
    }

    const currentGame = new Chess(fen);
    if (currentGame.turn() === playerColor) {
      return;
    }

    let cancelled = false;

    async function requestEngineMove() {
      setEngineBusy(true);

      try {
        const response = await fetch('/api/engine/move', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            fen: currentGame.fen(),
            playerColor
          })
        });

        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || 'Engine request failed.');
        }

        if (cancelled) {
          return;
        }

        const nextGame = new Chess(payload.fen);
        setFen(nextGame.fen());
        setSelectedSquare(null);
        setLegalTargets([]);
        setActiveClock(nextGame.isGameOver() ? null : nextGame.turn());

        if (payload.san?.includes('x')) {
          soundPlayerRef.current.capture();
        } else {
          soundPlayerRef.current.move();
        }

        if (payload.isCheck) {
          soundPlayerRef.current.check();
        }

        if (payload.result) {
          setResult(payload.result);
          setActiveClock(null);
          playResultSound(payload.result);
        }

        checkHealth();
      } catch (error) {
        if (!cancelled) {
          setResult({
            title: 'Engine error',
            detail: error.message
          });
          setActiveClock(null);
        }
      } finally {
        if (!cancelled) {
          setEngineBusy(false);
        }
      }
    }

    requestEngineMove();

    return () => {
      cancelled = true;
    };
  }, [fen, playerColor, result, showSidePicker]);

  function resetForNewGame(keepPlayerColor = false) {
    const freshGame = new Chess();
    setFen(freshGame.fen());
    setPlayerColor(keepPlayerColor ? playerColor : null);
    setSelectedSquare(null);
    setLegalTargets([]);
    setEngineBusy(false);
    setResult(null);
    setTimers({ w: INITIAL_TIME_MS, b: INITIAL_TIME_MS });
    setActiveClock(null);
  }

  function startGame(side) {
    resetForNewGame(true);
    setPlayerColor(side);
    setActiveClock('w');
    setShowSidePicker(false);
    soundPlayerRef.current.start();
  }

  function restartGame() {
    resetForNewGame();
    setShowSidePicker(true);
  }

  function openSidePicker() {
    restartGame();
  }

  function handleResign() {
    if (!playerColor || result || showSidePicker) {
      return;
    }

    const resignResult = {
      title: 'You lose',
      detail: 'You resigned.'
    };

    setResult(resignResult);
    setEngineBusy(false);
    setActiveClock(null);
    playResultSound(resignResult);
  }

  function handleSquareClick(square) {
    if (!playerColor || result || engineBusy || showSidePicker) {
      return;
    }

    const liveGame = new Chess(fen);
    if (liveGame.turn() !== playerColor) {
      return;
    }

    const clickedPiece = liveGame.get(square);

    if (selectedSquare === square) {
      setSelectedSquare(null);
      setLegalTargets([]);
      return;
    }

    if (selectedSquare) {
      const nextGame = new Chess(fen);
      const move = nextGame.move({
        from: selectedSquare,
        to: square,
        promotion: 'q'
      });

      if (move) {
        setFen(nextGame.fen());
        setSelectedSquare(null);
        setLegalTargets([]);

        if (move.captured) {
          soundPlayerRef.current.capture();
        } else {
          soundPlayerRef.current.move();
        }

        if (nextGame.isCheck()) {
          soundPlayerRef.current.check();
        }

        const nextResult = evaluatePosition(nextGame, playerColor);
        if (nextResult) {
          setResult(nextResult);
          setActiveClock(null);
          playResultSound(nextResult);
        } else {
          setActiveClock(nextGame.turn());
        }

        return;
      }
    }

    if (clickedPiece?.color === playerColor) {
      const moves = liveGame.moves({
        square,
        verbose: true
      });

      setSelectedSquare(square);
      setLegalTargets(moves.map((move) => move.to));
      return;
    }

    setSelectedSquare(null);
    setLegalTargets([]);
  }

  const squareStyles = {};

  if (selectedSquare) {
    squareStyles[selectedSquare] = {
      boxShadow: 'inset 0 0 0 4px rgba(208, 149, 21, 0.9)'
    };
  }

  for (const target of legalTargets) {
    squareStyles[target] = {
      ...(squareStyles[target] || {}),
      boxShadow: 'inset 0 0 0 3px rgba(52, 130, 87, 0.55)'
    };
  }

  const chessboardOptions = {
    position: fen,
    boardOrientation: playerColor === 'b' ? 'black' : 'white',
    showNotation: false,
    allowDragging: false,
    allowDrawingArrows: false,
    animationDurationInMs: 180,
    darkSquareStyle: { backgroundColor: '#d2a66e' },
    lightSquareStyle: { backgroundColor: '#f0d9b5' },
    squareStyles,
    boardStyle: {
      borderRadius: '6px',
      overflow: 'hidden',
      boxShadow: 'inset 0 0 0 1px rgba(0, 0, 0, 0.15)'
    },
    onSquareClick: ({ square }) => handleSquareClick(square)
  };

  const boardShellStyle =
    boardPixels > 0
      ? {
          width: `${boardPixels}px`,
          height: `${boardPixels}px`
        }
      : undefined;

  return (
    <main className="app-shell">
      <div className="glow glow-one" />
      <div className="glow glow-two" />

      <div className="game-layout">
        <section className="board-panel">
          <div className="board-holder" ref={boardStageRef}>
            <div className="board-shell" style={boardShellStyle}>
              <Chessboard options={chessboardOptions} />
            </div>
          </div>
        </section>

        <aside className="control-panel">
          <div className="control-card">
            <h1>CHESS DUEL Pro</h1>

            <div className="timer-grid">
              <div className={`timer-card ${activeClock === 'w' ? 'active' : ''}`}>
                <strong>{formatClock(timers.w)}</strong>
                <span>{playerColor === 'w' ? 'YOU (W)' : 'ENGINE (W)'}</span>
              </div>
              <div className={`timer-card ${activeClock === 'b' ? 'active' : ''}`}>
                <strong>{formatClock(timers.b)}</strong>
                <span>{playerColor === 'b' ? 'YOU (B)' : 'ENGINE (B)'}</span>
              </div>
            </div>

            <section className="game-control-card">
              <h2>Game Control</h2>

              <div className="status-lines">
                <div className="status-line-row">
                  <span>YOUR TURN:</span>
                  <strong>{game.turn() === 'w' ? 'White' : 'Black'}</strong>
                </div>
                <div className="status-line-row">
                  <span>YOU PLAY:</span>
                  <strong>
                    {playerColor === 'w'
                      ? 'White'
                      : playerColor === 'b'
                        ? 'Black'
                        : '-'}
                  </strong>
                </div>
                <div className="status-line-row">
                  <span>BACKEND:</span>
                  <strong className={health.online ? 'connected' : 'disconnected'}>
                    <span className="status-dot" />
                    {health.online ? 'Connected' : 'Offline'}
                  </strong>
                </div>
                <div className="status-line-row">
                  <span>ENGINE:</span>
                  <strong className={engineReady ? 'connected' : 'disconnected'}>
                    <span className="status-dot" />
                    {engineReady ? 'Connected' : 'Setup needed'}
                  </strong>
                </div>
              </div>

              <p className="status-message">{statusLine}</p>

              {result ? (
                <div
                  className={[
                    'result-card',
                    result.title === 'You win'
                      ? 'win'
                      : result.title === 'You lose'
                        ? 'lose'
                        : 'draw'
                  ].join(' ')}
                >
                  <strong>{result.title}</strong>
                  <p>{result.detail}</p>
                </div>
              ) : null}

              <div className="action-row">
                <button
                  className="primary-button"
                  onClick={restartGame}
                  type="button"
                >
                  Restart Game
                </button>
                <button
                  className="secondary-button"
                  disabled={!playerColor || Boolean(result) || showSidePicker}
                  onClick={handleResign}
                  type="button"
                >
                  Resign
                </button>
              </div>
            </section>

            <section className="side-picker-card">
              <h2>{showSidePicker ? 'Choose Your Side' : 'Current Side'}</h2>

              {showSidePicker ? (
                <>
                  <p className="side-picker-text">
                    Start a fresh 10-minute game as White or Black.
                  </p>

                  <div className="action-row side-picker-actions">
                    <button
                      className="primary-button"
                      disabled={!engineReady}
                      onClick={() => startGame('w')}
                      type="button"
                    >
                      Play White
                    </button>
                    <button
                      className="secondary-button"
                      disabled={!engineReady}
                      onClick={() => startGame('b')}
                      type="button"
                    >
                      Play Black
                    </button>
                  </div>

                  {!engineReady ? (
                    <p className="warning inline-warning">
                      {health.loading
                        ? 'Checking backend...'
                        : 'Engine is not ready yet. Use Refresh after the server is available.'}
                    </p>
                  ) : null}
                </>
              ) : (
                <>
                  <p className="side-picker-text">
                    You are currently playing as{' '}
                    <strong>{playerColor === 'w' ? 'White' : 'Black'}</strong>.
                  </p>

                  <button
                    className="secondary-button full-width-button"
                    onClick={openSidePicker}
                    type="button"
                  >
                    Change Side
                  </button>
                </>
              )}
            </section>
          </div>
        </aside>
      </div>
    </main>
  );
}
