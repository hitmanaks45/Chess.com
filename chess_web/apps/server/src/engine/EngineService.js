import { spawn } from 'node:child_process';
import path from 'node:path';

const BEST_MOVE_PATTERNS = [
  /\b(?:best\s*move|bestmove)\s*:?\s*([a-h][1-8][a-h][1-8][qrbn]?)\b/i,
  /^([a-h][1-8][a-h][1-8][qrbn]?)$/i
];

function parseBestMove(line) {
  for (const pattern of BEST_MOVE_PATTERNS) {
    const match = line.match(pattern);
    if (match) {
      return match[1].toLowerCase();
    }
  }

  return null;
}

export class EngineService {
  constructor(config) {
    this.config = config;
    this.child = null;
    this.queue = [];
    this.activeRequest = null;
    this.stdoutBuffer = '';
    this.ready = false;
  }

  isConfigured() {
    return Boolean(this.config.enginePath);
  }

  health() {
    return {
      configured: this.isConfigured(),
      ready: this.ready && Boolean(this.child),
      path: this.config.enginePath || null,
      queueDepth: this.queue.length + (this.activeRequest ? 1 : 0)
    };
  }

  async requestBestMove(fen) {
    if (!this.isConfigured()) {
      throw new Error('ENGINE_PATH is not configured. Add it to your .env file first.');
    }

    await this.start();

    return new Promise((resolve, reject) => {
      this.queue.push({ fen, resolve, reject });
      this.processQueue();
    });
  }

  async start() {
    if (this.child) {
      return;
    }

    const workingDirectory =
      this.config.engineWorkdir ||
      path.dirname(this.config.enginePath);

    this.child = spawn(this.config.enginePath, this.config.engineArgs, {
      cwd: workingDirectory,
      stdio: ['pipe', 'pipe', 'pipe']
    });

    this.ready = true;

    this.child.stdout.on('data', (chunk) => {
      this.stdoutBuffer += chunk.toString();

      let newlineIndex = this.stdoutBuffer.indexOf('\n');
      while (newlineIndex >= 0) {
        const rawLine = this.stdoutBuffer.slice(0, newlineIndex);
        this.stdoutBuffer = this.stdoutBuffer.slice(newlineIndex + 1);
        this.consumeOutputLine(rawLine.trim());
        newlineIndex = this.stdoutBuffer.indexOf('\n');
      }
    });

    this.child.stderr.on('data', (chunk) => {
      const text = chunk.toString().trim();
      if (text) {
        console.error(`[engine stderr] ${text}`);
      }
    });

    this.child.on('error', (error) => {
      this.ready = false;
      this.failAllPending(`Failed to start engine process: ${error.message}`);
    });

    this.child.on('exit', (code, signal) => {
      const reason =
        code !== null
          ? `Engine process exited with code ${code}.`
          : `Engine process exited from signal ${signal}.`;

      this.ready = false;
      this.child = null;
      this.failAllPending(reason);
    });
  }

  stop() {
    if (!this.child) {
      return;
    }

    this.child.kill();
    this.child = null;
    this.ready = false;
  }

  processQueue() {
    if (!this.child || this.activeRequest || this.queue.length === 0) {
      return;
    }

    this.activeRequest = this.queue.shift();
    const currentRequest = this.activeRequest;

    currentRequest.timeout = setTimeout(() => {
      if (this.activeRequest === currentRequest) {
        currentRequest.reject(
          new Error(
            `Engine move timed out after ${this.config.moveTimeoutMs}ms.`
          )
        );
        this.activeRequest = null;
        this.processQueue();
      }
    }, this.config.moveTimeoutMs);

    this.child.stdin.write(`${currentRequest.fen}\n`);
  }

  consumeOutputLine(line) {
    if (!line) {
      return;
    }

    const bestMove = parseBestMove(line);
    if (!bestMove || !this.activeRequest) {
      return;
    }

    clearTimeout(this.activeRequest.timeout);
    this.activeRequest.resolve(bestMove);
    this.activeRequest = null;
    this.processQueue();
  }

  failAllPending(message) {
    if (this.activeRequest) {
      clearTimeout(this.activeRequest.timeout);
      this.activeRequest.reject(new Error(message));
      this.activeRequest = null;
    }

    while (this.queue.length > 0) {
      const request = this.queue.shift();
      request.reject(new Error(message));
    }
  }
}
