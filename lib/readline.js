// Insperation for this code comes from Salvatore Sanfilippo's linenoise.
// http://github.com/antirez/linenoise
// Reference:
// * http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// * http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html

var kHistorySize = 30;
var kBufSize = 10*1024;


var Buffer = require('buffer').Buffer;
var inherits = require('sys').inherits;
var EventEmitter = require('events').EventEmitter;
var stdio = process.binding('stdio');



exports.createInterface = function (output, isTTY) {
  return new Interface(output, isTTY);
};

function Interface (output, isTTY) {
  this.output = output;

  this.setPrompt("node> ");

  // Current line
  this.buf = new Buffer(kBufSize);
  this.buf.used = 0;

  if (!isTTY) {
    this._tty = false;
  } else {
    // input refers to stdin

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
    this.buf.used = 0;
    this.cursor = 0;
    this._refreshLine();
  } else {
    this.output.write(this._prompt);
  }
};


Interface.prototype._addHistory = function () {
  if (this.buf.used === 0) return "";

  var b = new Buffer(this.buf.used);
  this.buf.copy(b, 0, 0, this.buf.used);
  this.buf.used = 0;

  this.history.unshift(b);
  this.historyIndex = -1;

  this.cursor = 0;

  // Only store so many
  if (this.history.length > kHistorySize) this.history.pop();

  return b.toString('utf8');
};


Interface.prototype._refreshLine  = function () {
  if (this._closed) return;

  stdio.setRawMode(true);

  // Cursor to left edge.
  this.output.write('\x1b[0G');

  // Write the prompt and the current buffer content.
  this.output.write(this._prompt);
  if (this.buf.used > 0) {
    this.output.write(this.buf.slice(0, this.buf.used));
  }

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
  for (var i = 0; i < b.length; i++) {
    var code = b instanceof Buffer ? b[i] : b.charCodeAt(i);
    if (code === '\n'.charCodeAt(0) || code === '\r'.charCodeAt(0)) {
      var s = this.buf.toString('utf8', 0, this.buf.used);
      this.emit('line', s);
      this.buf.used = 0;
    } else {
      this.buf[this.buf.used++] = code;
    }
  }
};


Interface.prototype._ttyWrite = function (b) {
  switch (b[0]) {
    /* ctrl+c */
    case 3:
      //process.kill(process.pid, "SIGINT");
      this.close();
      break;

    case 4:     /* ctrl+d */
      this.close();
      break;

    case 13:    /* enter */
      var line = this._addHistory();
      this.output.write('\n\x1b[0G');
      stdio.setRawMode(false);
      this.emit('line', line);
      break;

    case 127:   /* backspace */
    case 8:     /* ctrl+h */
      if (this.cursor > 0 && this.buf.used > 0) {
        for (var i = this.cursor; i < this.buf.used; i++) {
          this.buf[i-1] = this.buf[i];
        }
        this.cursor--;
        this.buf.used--;
        this._refreshLine();
      }
      break;

    case 21: /* Ctrl+u, delete the whole line. */
      this.cursor = this.buf.used = 0;
      this._refreshLine();
      break;

    case 11: /* Ctrl+k, delete from current to end of line. */
      this.buf.used = this.cursor;
      this._refreshLine();
      break;

    case 1: /* Ctrl+a, go to the start of the line */
      this.cursor = 0;
      this._refreshLine();
      break;

    case 5: /* ctrl+e, go to the end of the line */
      this.cursor = this.buf.used;
      this._refreshLine();
      break;

    case 27:    /* escape sequence */
      if (b[1] === 91 && b[2] === 68) {
        // left arrow
        if (this.cursor > 0) {
          this.cursor--;
          this._refreshLine();
        }
      } else if (b[1] === 91 && b[2] === 67) {
        // right arrow
        if (this.cursor != this.buf.used) {
          this.cursor++;
          this._refreshLine();
        }
      } else if (b[1] === 91 && b[2] === 65) {
        // up arrow
        if (this.historyIndex + 1 < this.history.length) {
          this.historyIndex++;
          this.history[this.historyIndex].copy(this.buf, 0);
          this.buf.used = this.history[this.historyIndex].length;
          // set cursor to end of line.
          this.cursor = this.buf.used;

          this._refreshLine();
        }

      } else if (b[1] === 91 && b[2] === 66) {
        // down arrow
        if (this.historyIndex > 0) {
          this.historyIndex--;
          this.history[this.historyIndex].copy(this.buf, 0);
          this.buf.used = this.history[this.historyIndex].length;
          // set cursor to end of line.
          this.cursor = this.buf.used;
          this._refreshLine();

        } else if (this.historyIndex === 0) {
          this.historyIndex = -1;
          this.cursor = 0;
          this.buf.used = 0;
          this._refreshLine();
        }
      }
      break;

    default:
      if (this.buf.used < kBufSize) {
        for (var i = this.buf.used + 1; this.cursor < i; i--) {
          this.buf[i] = this.buf[i-1];
        }
        this.buf[this.cursor++] = b[0];
        this.buf.used++;

        if (this.buf.used == this.cursor) {
          this.output.write(b);
        } else {
          this._refreshLine();
        }
      }
      break;
  }
};

