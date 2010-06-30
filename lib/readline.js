// Inspiration for this code comes from Salvatore Sanfilippo's linenoise.
// http://github.com/antirez/linenoise
// Reference:
// * http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// * http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html

var kHistorySize = 30;
var kBufSize = 10*1024;


var Buffer = require('buffer').Buffer;
var sys = require('sys');
var inherits = require('sys').inherits;
var EventEmitter = require('events').EventEmitter;
var stdio = process.binding('stdio');



exports.createInterface = function (output) {
  return new Interface(output);
};

function Interface (output) {
  this.output = output;

  this.setPrompt("node> ");

  this._tty = output.fd < 3;

  if (parseInt(process.env['NODE_NO_READLINE'])) {
    this._tty = false;
  }

  if (this._tty) {
    // input refers to stdin

    // Current line
    this.line = "";

    // Check process.env.TERM ?
    stdio.setRawMode(true);
    this._tty = true;
    this.columns = stdio.getColumns();

    // Cursor position on the line.
    this.cursor = 0;

    this.history = [];
    this.historyIndex = -1;
  }
}

inherits(Interface, EventEmitter);


Interface.prototype.setPrompt = function (prompt, length) {
  this._prompt = prompt;
  this._promptLength = length ? length : Buffer.byteLength(prompt);
};


Interface.prototype.prompt  = function () {
  if (this._tty) {
    this.cursor = 0;
    this._refreshLine();
  } else {
    this.output.write(this._prompt);
  }
};


Interface.prototype._addHistory = function () {
  if (this.line.length === 0) return "";

  this.history.unshift(this.line);
  this.line = "";
  this.historyIndex = -1;

  this.cursor = 0;

  // Only store so many
  if (this.history.length > kHistorySize) this.history.pop();

  return this.history[0];
};


Interface.prototype._refreshLine  = function () {
  if (this._closed) return;

  stdio.setRawMode(true);

  // Cursor to left edge.
  this.output.write('\x1b[0G');

  // Write the prompt and the current buffer content.
  this.output.write(this._prompt);
  this.output.write(this.line);

  // Erase to right.
  this.output.write('\x1b[0K');

  // Move cursor to original position.
  this.output.write('\x1b[0G\x1b[' + (this._promptLength + this.cursor) + 'C');
};


Interface.prototype.close = function (d) {
  if (this._tty) {
    stdio.setRawMode(false);
  }
  this.emit('close');
  this._closed = true;
};


Interface.prototype.write = function (d) {
  if (this._closed) return;
  return this._tty ? this._ttyWrite(d) : this._normalWrite(d);
};


Interface.prototype._normalWrite = function (b) {
  // Very simple implementation right now. Should try to break on
  // new lines.
  this.emit('line', b.toString());
};

Interface.prototype._historyNext = function () {
  if (this.historyIndex > 0) {
    this.historyIndex--;
    this.line = this.history[this.historyIndex];
    this.cursor = this.line.length; // set cursor to end of line.
    this._refreshLine();

  } else if (this.historyIndex === 0) {
    this.historyIndex = -1;
    this.cursor = 0;
    this.line = '';
    this._refreshLine();
  }
};

Interface.prototype._historyPrev = function () {
  if (this.historyIndex + 1 < this.history.length) {
    this.historyIndex++;
    this.line = this.history[this.historyIndex];
    this.cursor = this.line.length; // set cursor to end of line.

    this._refreshLine();
  }
};

// handle a write from the tty
Interface.prototype._ttyWrite = function (b) {
  switch (b[0]) {
    /* ctrl+c */
    case 3:
      //process.kill(process.pid, "SIGINT");
      this.close();
      break;

    case 4: // control-d, delete right or EOF
      if (this.cursor === 0 && this.line.length === 0) {
        this.close();
      } else if (this.cursor < this.line.length) {
        this.line = this.line.slice(0, this.cursor)
                  + this.line.slice(this.cursor+1, this.line.length)
                  ;
        this._refreshLine();
      }
      break;

    case 13:    /* enter */
      var line = this._addHistory();
      this.output.write('\n\x1b[0G');
      stdio.setRawMode(false);
      this.emit('line', line);
      break;

    case 127:   /* backspace */
    case 8:     /* ctrl+h */
      if (this.cursor > 0 && this.line.length > 0) {
        this.line = this.line.slice(0, this.cursor-1)
                  + this.line.slice(this.cursor, this.line.length)
                  ;
        this.cursor--;
        this._refreshLine();
      }
      break;

    case 21: /* Ctrl+u, delete the whole line. */
      this.cursor = 0;
      this.line = '';
      this._refreshLine();
      break;

    case 11: /* Ctrl+k, delete from current to end of line. */
      this.line = this.line.slice(0, this.cursor);
      this._refreshLine();
      break;

    case 1: /* Ctrl+a, go to the start of the line */
      this.cursor = 0;
      this._refreshLine();
      break;

    case 5: /* ctrl+e, go to the end of the line */
      this.cursor = this.line.length;
      this._refreshLine();
      break;

    case 2: // control-b, back one character
      if (this.cursor > 0) {
        this.cursor--;
        this._refreshLine();
      }
      break;

    case 6: // control-f, forward one character
      if (this.cursor != this.line.length) {
        this.cursor++;
        this._refreshLine();
      }
      break;

    case 14: // control-n, next history item
      this._historyNext();
      break;

    case 16: // control-p, previous history item
      this._historyPrev();
      break;

    case 26:    /* ctrl+z */
      process.kill(process.pid, "SIGTSTP");
      return;

    case 27:    /* escape sequence */
      if (b[1] === 98 && this.cursor > 0) { // meta-b - backward word

      } else if (b[1] === 102 && this.cursor < this.line.length) { // meta-f - forward word

      } else if (b[1] === 91 && b[2] === 68) { // left arrow
        if (this.cursor > 0) {
          this.cursor--;
          this.output.write('\x1b[0D');
        }
      } else if (b[1] === 91 && b[2] === 67) { // right arrow
        if (this.cursor != this.line.length) {
          this.cursor++;
          this.output.write('\x1b[0C');
        }
      } else if (b[1] === 91 && b[2] === 65) { // up arrow
        this._historyPrev();
      } else if (b[1] === 91 && b[2] === 66) { // down arrow
        this._historyNext();
      }
      break;

    default:
      var c = b.toString('utf8');
      if (this.cursor < this.line.length) {
        var beg = this.line.slice(0, this.cursor);
        var end = this.line.slice(this.cursor, this.line.length);
        this.line = beg + c + end;
        this.cursor += c.length;
        this._refreshLine();
      } else {
        this.line += c;
        this.cursor += c.length;
        this.output.write(c);
      }
      break;
  }
};

