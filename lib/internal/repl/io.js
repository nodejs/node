'use strict';

const { emitKeys, CSI, cursorTo } = require('internal/repl/tty');

class IO {
  constructor(stdout, stdin, onLine, onAutocomplete, transformBuffer, heading) {
    this.stdin = stdin;
    this.stdout = stdout;

    this.buffer = '';
    this.cursor = 0;
    this.prefix = '';
    this.suffix = '';

    this._paused = false;
    this.transformBuffer = transformBuffer;
    this.completionList = undefined;

    this.onAutocomplete = onAutocomplete;

    this.history = [];
    this.history.index = -1;

    let closeOnThisOne = false;

    stdin.cork();
    this.clear();
    stdout.write(`${heading}\n`);

    const decoder = emitKeys(async (s, key) => {
      if (key.ctrl) {
        switch (key.name) {
          case 'h':
            break;
          case 'u':
            this.buffer = this.buffer.slice(this.cursor, this.buffer.length);
            this.cursor = 0;
            await this.refresh();
            break;
          case 'k':
            this.buffer = this.buffer.slice(0, this.cursor);
            this.cursor = this.buffer.length;
            await this.refresh();
            break;
          case 'a':
            await this.moveCursor(-Infinity);
            break;
          case 'e':
            await this.moveCursor(Infinity);
            break;
          case 'b':
            await this.moveCursor(-1);
            break;
          case 'f':
            await this.moveCursor(1);
            break;
          case 'l':
            cursorTo(stdout, 0, 0);
            stdout.write(CSI.kClearScreenDown);
            await this.refresh();
            break;
          case 'n':
            await this.nextHistory();
            break;
          case 'p':
            await this.previousHistory();
            break;
          case 'c':
            if (closeOnThisOne) {
              return -1;
            }
            this.stdout.write('\n(To exit, press ^C again or call exit)\n');
            this.buffer = '';
            this.cursor = 0;
            closeOnThisOne = true;
            return undefined;
          case 'z':
          case 'd':
            return -1;
          default:
            break;
        }
        return undefined;
      }

      switch (key.name) {
        case 'up':
          await this.previousHistory();
          break;
        case 'down':
          await this.nextHistory();
          break;
        case 'left':
          await this.moveCursor(-1);
          break;
        case 'right':
          if (this.cursor === this.buffer.length) {
            if (this.suffix) {
              this.completionList = undefined;
              this.buffer += this.suffix;
              this.cursor += this.suffix.length;
              await this.refresh();
            }
            break;
          }
          await this.moveCursor(1);
          break;
        case 'delete':
        case 'backspace':
          if (this.cursor === 0) {
            break;
          }
          this.buffer = this.buffer.slice(0, this.cursor - 1) +
            this.buffer.slice(this.cursor, this.buffer.length);
          await this.moveCursor(-1);
          break;
        case 'tab': {
          await this.autocomplete();
          break;
        }
        default:
          if (s) {
            this.history.index = -1;
            const lines = s.split(/\r\n|\n|\r/);
            for (var i = 0; i < lines.length; i += 1) {
              if (i > 0) {
                if (this.buffer) {
                  await this.refresh(false);
                  this.pause();
                  this.stdout.write('\n');
                  const b = this.buffer;
                  this.buffer = '';
                  this.cursor = 0;
                  this.history.unshift(b);
                  this.stdout.write(`${await onLine(b)}\n`);
                  this.unpause();
                } else {
                  this.stdout.write('\n');
                }
                await this.refresh();
              }
              await this.appendToBuffer(lines[i]);
            }
          }
          break;
      }
      return undefined;
    });

    decoder.next('');

    stdout.setEncoding('utf8');

    (async () => {
      if (stdin.setRawMode) {
        stdin.setRawMode(true);
      }
      stdin.setEncoding('utf8');
      this.unpause();
      const handle = async (data) => {
        for (var i = 0; i < data.length; i += 1) {
          const { value } = await decoder.next(data[i]);
          if (value === -1) {
            process.exit(0);
          }
          process._tickCallback();
        }
        stdin.once('data', handle);
      };
      stdin.once('data', handle);
    })().catch((err) => {
      console.error(err); // eslint-disable-line no-console
      process.exit(1);
    });
  }

  get paused() {
    return this._paused;
  }

  pause() {
    this._paused = true;
    this.stdin.cork();
  }

  unpause() {
    this.stdin.uncork();
    this._paused = false;
  }

  async nextHistory() {
    if (this.history.index <= 0) {
      this.buffer = '';
      this.cursor = 0;
      await this.refresh();
      return;
    }
    this.history.index -= 1;
    this.buffer = this.history[this.history.index];
    this.cursor = this.buffer.length;
    await this.refresh();
  }

  async previousHistory() {
    if (this.history.index >= this.history.length - 1) {
      return;
    }
    this.history.index += 1;
    this.buffer = this.history[this.history.index];
    this.cursor = this.buffer.length;
    await this.refresh();
  }

  async autocomplete() {
    if (!this.onAutocomplete) {
      return;
    }
    if (this.completionList && this.completionList.length) {
      const next = this.completionList.shift();
      await this.addSuffix(next);
    } else if (this.completionList) {
      this.completionList = undefined;
      await this.refresh(false);
    } else {
      const c = await this.onAutocomplete(this.buffer);
      if (c) {
        this.completionList = c;
        await this.autocomplete();
      }
    }
  }

  async setPrefix(s) {
    if (!s) {
      this.prefix = '';
    }
    this.prefix = s;
    await this.refresh();
  }

  async addSuffix(s = '') {
    if (this.paused || this.cursor !== this.buffer.length) {
      return;
    }
    this.suffix = `${s}`;
    this.stdout.write(CSI.kClearScreenDown);
    this.stdout.write(`\u001b[90m${this.suffix}\u001b[39m`);
    cursorTo(this.stdout, this.cursor + this.prefix.length);
  }

  async appendToBuffer(s) {
    if (this.cursor < this.buffer.length) {
      const beg = this.buffer.slice(0, this.cursor);
      const end = this.buffer.slice(this.cursor, this.buffer.length);
      this.buffer = beg + s + end;
    } else {
      this.buffer += s;
    }

    this.cursor += s.length;
    await this.refresh();
  }

  async moveCursor(n) {
    const c = this.cursor + n;
    if (c < 0) {
      this.cursor = 0;
    } else if (c > this.buffer.length) {
      this.cursor = this.buffer.length;
    } else {
      this.cursor = c;
    }
    await this.refresh();
  }

  clear() {
    cursorTo(this.stdout, 0);
    this.stdout.write(CSI.kClearScreenDown);
  }

  async refresh(complete = true) {
    if (this.paused) {
      return;
    }
    this.completionList = undefined;
    this.suffix = '';
    this.clear();
    const b = this.transformBuffer ?
      await this.transformBuffer(this.buffer) :
      this.buffer;
    this.stdout.write(this.prefix + b);
    cursorTo(this.stdout, this.cursor + this.prefix.length);
    if (complete && this.buffer) {
      await this.autocomplete();
    }
  }
}

module.exports = IO;
