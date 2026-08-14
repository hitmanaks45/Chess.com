# Chess Web Suite

This folder is a brand-new React + Node.js chess game and does not modify your C++ engine project.

## What this gives you

- A clean React chess UI with:
  - side selection (`White` or `Black`)
  - fixed `10:00` timer for both sides
  - square highlight and deselect on second click
  - legal-move execution
  - engine turn loop
  - win / lose / draw overlay
  - restart flow
  - synthesized move / capture / check / result sounds
- A persistent Node.js engine service that keeps your C++ engine alive as a child process
- A very simple protocol bridge:
  - server sends one full FEN line to `stdin`
  - engine replies with a line such as `Best move : e2e4`

## Folder layout

```text
apps/
  server/   Node + Express engine bridge
  web/      React + Vite chess client
```

## Engine expectations

Your engine should behave like this:

1. Read a FEN from standard input.
2. Compute the best move.
3. Print a move line like `Best move : e2e4`.

The bridge also accepts `bestmove e2e4` or just `e2e4`.

## Setup

1. Build your C++ engine separately in its own project folder.
2. Copy `.env.example` to `.env`.
3. Set `ENGINE_PATH` to your compiled engine executable.
4. Run `npm install`.

## Development

```bash
npm run dev
```

This starts:

- the API server on `http://localhost:3000`
- the React client on `http://localhost:5173`

The Vite client opens automatically in your browser.

## Production-style local run

```bash
npm run build
npm start
```

The Node server serves the built React app and can open the browser automatically when `AUTO_OPEN_BROWSER=true`.

## Notes

- The web app stays completely separate from your engine source tree.
- Only the engine executable path is required.
- If you later want a standalone desktop window, the current structure is already a good base for adding Electron on top without changing the game logic.
