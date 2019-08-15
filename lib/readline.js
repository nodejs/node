// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Inspiration for this code comes from Salvatore Sanfilippo's linenoise.
// https://github.com/antirez/linenoise
// Reference:
// * http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// * http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html

'use strict';

const { Math, Object } = primordials;

const {
  ERR_INVALID_CALLBACK,
  ERR_INVALID_CURSOR_POS,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const { inspect } = require('internal/util/inspect');
const EventEmitter = require('events');
const {
  CSI,
  emitKeys,
  getStringWidth,
  isFullWidthCodePoint,
  kUTF16SurrogateThreshold,
  stripVTControlCharacters
} = require('internal/readline/utils');

const { clearTimeout, setTimeout } = require('timers');
const {
  kEscape,
  kClearToBeginning,
  kClearToEnd,
  kClearLine,
  kClearScreenDown
} = CSI;

// Lazy load StringDecoder for startup performance.
let StringDecoder;

// Lazy load Readable for startup performance.
let Readable;

const kHistorySize = 30;
const kMincrlfDelay = 100;
// \r\n, \n, or \r followed by something other than \n
const lineEnding = /\r?\n|\r(?!\n)/;

const kLineObjectStream = Symbol('line object stream');

const KEYPRESS_DECODER = Symbol('keypress-decoder');
const ESCAPE_DECODER = Symbol('escape-decoder');

// GNU readline library - keyseq-timeout is 500ms (default)
const ESCAPE_CODE_TIMEOUT = 500;

function createInterface(input, output, completer, terminal) {
  return new Interface(input, output, completer, terminal);
}


function Interface(input, output, completer, terminal) {
  if (!(this instanceof Interface)) {
    return new Interface(input, output, completer, terminal);
  }

  if (StringDecoder === undefined)
    StringDecoder = require('string_decoder').StringDecoder;

  this._sawReturnAt = 0;
  this.isCompletionEnabled = true;
  this._sawKeyPress = false;
  this._previousKey = null;
  this.escapeCodeTimeout = ESCAPE_CODE_TIMEOUT;

  EventEmitter.call(this);
  var historySize;
  var removeHistoryDuplicates = false;
  let crlfDelay;
  let prompt = '> ';

  if (input && input.input) {
    // An options object was given
    output = input.output;
    completer = input.completer;
    terminal = input.terminal;
    historySize = input.historySize;
    removeHistoryDuplicates = input.removeHistoryDuplicates;
    if (input.prompt !== undefined) {
      prompt = input.prompt;
    }
    if (input.escapeCodeTimeout !== undefined) {
      if (Number.isFinite(input.escapeCodeTimeout)) {
        this.escapeCodeTimeout = input.escapeCodeTimeout;
      } else {
        throw new ERR_INVALID_OPT_VALUE(
          'escapeCodeTimeout',
          this.escapeCodeTimeout
        );
      }
    }
    crlfDelay = input.crlfDelay;
    input = input.input;
  }

  if (completer !== undefined && typeof completer !== 'function') {
    throw new ERR_INVALID_OPT_VALUE('completer', completer);
  }

  if (historySize === undefined) {
    historySize = kHistorySize;
  }

  if (typeof historySize !== 'number' ||
      Number.isNaN(historySize) ||
      historySize < 0) {
    throw new ERR_INVALID_OPT_VALUE.RangeError('historySize', historySize);
  }

  // Backwards compat; check the isTTY prop of the output stream
  //  when `terminal` was not specified
  if (terminal === undefined && !(output === null || output === undefined)) {
    terminal = !!output.isTTY;
  }

  const self = this;

  this.output = output;
  this.input = input;
  this.historySize = historySize;
  this.removeHistoryDuplicates = !!removeHistoryDuplicates;
  this.crlfDelay = crlfDelay ?
    Math.max(kMincrlfDelay, crlfDelay) : kMincrlfDelay;
  // Check arity, 2 - for async, 1 for sync
  if (typeof completer === 'function') {
    this.completer = completer.length === 2 ?
      completer :
      function completerWrapper(v, cb) {
        cb(null, completer(v));
      };
  }

  this.setPrompt(prompt);

  this.terminal = !!terminal;

  if (process.env.TERM === 'dumb') {
    this._ttyWrite = _ttyWriteDumb.bind(this);
  }

  function ondata(data) {
    self._normalWrite(data);
  }

  function onend() {
    if (typeof self._line_buffer === 'string' &&
        self._line_buffer.length > 0) {
      self.emit('line', self._line_buffer);
    }
    self.close();
  }

  function ontermend() {
    if (typeof self.line === 'string' && self.line.length > 0) {
      self.emit('line', self.line);
    }
    self.close();
  }

  function onkeypress(s, key) {
    self._ttyWrite(s, key);
    if (key && key.sequence) {
      // If the key.sequence is half of a surrogate pair
      // (>= 0xd800 and <= 0xdfff), refresh the line so
      // the character is displayed appropriately.
      const ch = key.sequence.codePointAt(0);
      if (ch >= 0xd800 && ch <= 0xdfff)
        self._refreshLine();
    }
  }

  function onresize() {
    self._refreshLine();
  }

  this[kLineObjectStream] = undefined;

  if (!this.terminal) {
    function onSelfCloseWithoutTerminal() {
      input.removeListener('data', ondata);
      input.removeListener('end', onend);
    }

    input.on('data', ondata);
    input.on('end', onend);
    self.once('close', onSelfCloseWithoutTerminal);
    this._decoder = new StringDecoder('utf8');
  } else {
    function onSelfCloseWithTerminal() {
      input.removeListener('keypress', onkeypress);
      input.removeListener('end', ontermend);
      if (output !== null && output !== undefined) {
        output.removeListener('resize', onresize);
      }
    }

    emitKeypressEvents(input, this);

    // `input` usually refers to stdin
    input.on('keypress', onkeypress);
    input.on('end', ontermend);

    // Current line
    this.line = '';

    this._setRawMode(true);
    this.terminal = true;

    // Cursor position on the line.
    this.cursor = 0;

    this.history = [];
    this.historyIndex = -1;

    if (output !== null && output !== undefined)
      output.on('resize', onresize);

    self.once('close', onSelfCloseWithTerminal);
  }

  input.resume();
}

Object.setPrototypeOf(Interface.prototype, EventEmitter.prototype);
Object.setPrototypeOf(Interface, EventEmitter);

Object.defineProperty(Interface.prototype, 'columns', {
  configurable: true,
  enumerable: true,
  get: function() {
    var columns = Infinity;
    if (this.output && this.output.columns)
      columns = this.output.columns;
    return columns;
  }
});

Interface.prototype.setPrompt = function(prompt) {
  this._prompt = prompt;
};


Interface.prototype._setRawMode = function(mode) {
  const wasInRawMode = this.input.isRaw;

  if (typeof this.input.setRawMode === 'function') {
    this.input.setRawMode(mode);
  }

  return wasInRawMode;
};


Interface.prototype.prompt = function(preserveCursor) {
  if (this.paused) this.resume();
  if (this.terminal && process.env.TERM !== 'dumb') {
    if (!preserveCursor) this.cursor = 0;
    this._refreshLine();
  } else {
    this._writeToOutput(this._prompt);
  }
};


Interface.prototype.question = function(query, cb) {
  if (typeof cb === 'function') {
    if (this._questionCallback) {
      this.prompt();
    } else {
      this._oldPrompt = this._prompt;
      this.setPrompt(query);
      this._questionCallback = cb;
      this.prompt();
    }
  }
};


Interface.prototype._onLine = function(line) {
  if (this._questionCallback) {
    var cb = this._questionCallback;
    this._questionCallback = null;
    this.setPrompt(this._oldPrompt);
    cb(line);
  } else {
    this.emit('line', line);
  }
};

Interface.prototype._writeToOutput = function _writeToOutput(stringToWrite) {
  validateString(stringToWrite, 'stringToWrite');

  if (this.output !== null && this.output !== undefined) {
    this.output.write(stringToWrite);
  }
};

Interface.prototype._addHistory = function() {
  if (this.line.length === 0) return '';

  // If the history is disabled then return the line
  if (this.historySize === 0) return this.line;

  // If the trimmed line is empty then return the line
  if (this.line.trim().length === 0) return this.line;

  if (this.history.length === 0 || this.history[0] !== this.line) {
    if (this.removeHistoryDuplicates) {
      // Remove older history line if identical to new one
      const dupIndex = this.history.indexOf(this.line);
      if (dupIndex !== -1) this.history.splice(dupIndex, 1);
    }

    this.history.unshift(this.line);

    // Only store so many
    if (this.history.length > this.historySize) this.history.pop();
  }

  this.historyIndex = -1;
  return this.history[0];
};


Interface.prototype._refreshLine = function() {
  // line length
  const line = this._prompt + this.line;
  const dispPos = this._getDisplayPos(line);
  const lineCols = dispPos.cols;
  const lineRows = dispPos.rows;

  // cursor position
  const cursorPos = this._getCursorPos();

  // First move to the bottom of the current line, based on cursor pos
  const prevRows = this.prevRows || 0;
  if (prevRows > 0) {
    moveCursor(this.output, 0, -prevRows);
  }

  // Cursor to left edge.
  cursorTo(this.output, 0);
  // erase data
  clearScreenDown(this.output);

  // Write the prompt and the current buffer content.
  this._writeToOutput(line);

  // Force terminal to allocate a new line
  if (lineCols === 0) {
    this._writeToOutput(' ');
  }

  // Move cursor to original position.
  cursorTo(this.output, cursorPos.cols);

  const diff = lineRows - cursorPos.rows;
  if (diff > 0) {
    moveCursor(this.output, 0, -diff);
  }

  this.prevRows = cursorPos.rows;
};


Interface.prototype.close = function() {
  if (this.closed) return;
  this.pause();
  if (this.terminal) {
    this._setRawMode(false);
  }
  this.closed = true;
  this.emit('close');
};


Interface.prototype.pause = function() {
  if (this.paused) return;
  this.input.pause();
  this.paused = true;
  this.emit('pause');
  return this;
};


Interface.prototype.resume = function() {
  if (!this.paused) return;
  this.input.resume();
  this.paused = false;
  this.emit('resume');
  return this;
};


Interface.prototype.write = function(d, key) {
  if (this.paused) this.resume();
  if (this.terminal) {
    this._ttyWrite(d, key);
  } else {
    this._normalWrite(d);
  }
};

Interface.prototype._normalWrite = function(b) {
  if (b === undefined) {
    return;
  }
  var string = this._decoder.write(b);
  if (this._sawReturnAt &&
      Date.now() - this._sawReturnAt <= this.crlfDelay) {
    string = string.replace(/^\n/, '');
    this._sawReturnAt = 0;
  }

  // Run test() on the new string chunk, not on the entire line buffer.
  const newPartContainsEnding = lineEnding.test(string);

  if (this._line_buffer) {
    string = this._line_buffer + string;
    this._line_buffer = null;
  }
  if (newPartContainsEnding) {
    this._sawReturnAt = string.endsWith('\r') ? Date.now() : 0;

    // Got one or more newlines; process into "line" events
    var lines = string.split(lineEnding);
    // Either '' or (conceivably) the unfinished portion of the next line
    string = lines.pop();
    this._line_buffer = string;
    for (var n = 0; n < lines.length; n++)
      this._onLine(lines[n]);
  } else if (string) {
    // No newlines this time, save what we have for next time
    this._line_buffer = string;
  }
};

Interface.prototype._insertString = function(c) {
  if (this.cursor < this.line.length) {
    var beg = this.line.slice(0, this.cursor);
    var end = this.line.slice(this.cursor, this.line.length);
    this.line = beg + c + end;
    this.cursor += c.length;
    this._refreshLine();
  } else {
    this.line += c;
    this.cursor += c.length;

    if (this._getCursorPos().cols === 0) {
      this._refreshLine();
    } else {
      this._writeToOutput(c);
    }

    // A hack to get the line refreshed if it's needed
    this._moveCursor(0);
  }
};

Interface.prototype._tabComplete = function(lastKeypressWasTab) {
  const self = this;

  self.pause();
  self.completer(self.line.slice(0, self.cursor), function onComplete(err, rv) {
    self.resume();

    if (err) {
      self._writeToOutput(`tab completion error ${inspect(err)}`);
      return;
    }

    const completions = rv[0];
    const completeOn = rv[1];  // The text that was completed
    if (completions && completions.length) {
      // Apply/show completions.
      if (lastKeypressWasTab) {
        self._writeToOutput('\r\n');
        var width = completions.reduce(function completionReducer(a, b) {
          return a.length > b.length ? a : b;
        }).length + 2;  // 2 space padding
        var maxColumns = Math.floor(self.columns / width);
        if (!maxColumns || maxColumns === Infinity) {
          maxColumns = 1;
        }
        var group = [];
        for (var i = 0; i < completions.length; i++) {
          var c = completions[i];
          if (c === '') {
            handleGroup(self, group, width, maxColumns);
            group = [];
          } else {
            group.push(c);
          }
        }
        handleGroup(self, group, width, maxColumns);
      }

      // If there is a common prefix to all matches, then apply that portion.
      var f = completions.filter((e) => e);
      var prefix = commonPrefix(f);
      if (prefix.length > completeOn.length) {
        self._insertString(prefix.slice(completeOn.length));
      }

      self._refreshLine();
    }
  });
};

// this = Interface instance
function handleGroup(self, group, width, maxColumns) {
  if (group.length === 0) {
    return;
  }
  const minRows = Math.ceil(group.length / maxColumns);
  for (var row = 0; row < minRows; row++) {
    for (var col = 0; col < maxColumns; col++) {
      var idx = row * maxColumns + col;
      if (idx >= group.length) {
        break;
      }
      var item = group[idx];
      self._writeToOutput(item);
      if (col < maxColumns - 1) {
        for (var s = 0; s < width - item.length; s++) {
          self._writeToOutput(' ');
        }
      }
    }
    self._writeToOutput('\r\n');
  }
  self._writeToOutput('\r\n');
}

function commonPrefix(strings) {
  if (!strings || strings.length === 0) {
    return '';
  }
  if (strings.length === 1) return strings[0];
  const sorted = strings.slice().sort();
  const min = sorted[0];
  const max = sorted[sorted.length - 1];
  for (var i = 0, len = min.length; i < len; i++) {
    if (min[i] !== max[i]) {
      return min.slice(0, i);
    }
  }
  return min;
}


Interface.prototype._wordLeft = function() {
  if (this.cursor > 0) {
    // Reverse the string and match a word near beginning
    // to avoid quadratic time complexity
    var leading = this.line.slice(0, this.cursor);
    var reversed = leading.split('').reverse().join('');
    var match = reversed.match(/^\s*(?:[^\w\s]+|\w+)?/);
    this._moveCursor(-match[0].length);
  }
};


Interface.prototype._wordRight = function() {
  if (this.cursor < this.line.length) {
    var trailing = this.line.slice(this.cursor);
    var match = trailing.match(/^(?:\s+|[^\w\s]+|\w+)\s*/);
    this._moveCursor(match[0].length);
  }
};

function charLengthLeft(str, i) {
  if (i <= 0)
    return 0;
  if (i > 1 && str.codePointAt(i - 2) >= kUTF16SurrogateThreshold ||
      str.codePointAt(i - 1) >= kUTF16SurrogateThreshold) {
    return 2;
  }
  return 1;
}

function charLengthAt(str, i) {
  if (str.length <= i)
    return 0;
  return str.codePointAt(i) >= kUTF16SurrogateThreshold ? 2 : 1;
}

Interface.prototype._deleteLeft = function() {
  if (this.cursor > 0 && this.line.length > 0) {
    // The number of UTF-16 units comprising the character to the left
    const charSize = charLengthLeft(this.line, this.cursor);
    this.line = this.line.slice(0, this.cursor - charSize) +
                this.line.slice(this.cursor, this.line.length);

    this.cursor -= charSize;
    this._refreshLine();
  }
};


Interface.prototype._deleteRight = function() {
  if (this.cursor < this.line.length) {
    // The number of UTF-16 units comprising the character to the left
    const charSize = charLengthAt(this.line, this.cursor);
    this.line = this.line.slice(0, this.cursor) +
      this.line.slice(this.cursor + charSize, this.line.length);
    this._refreshLine();
  }
};


Interface.prototype._deleteWordLeft = function() {
  if (this.cursor > 0) {
    // Reverse the string and match a word near beginning
    // to avoid quadratic time complexity
    var leading = this.line.slice(0, this.cursor);
    var reversed = leading.split('').reverse().join('');
    var match = reversed.match(/^\s*(?:[^\w\s]+|\w+)?/);
    leading = leading.slice(0, leading.length - match[0].length);
    this.line = leading + this.line.slice(this.cursor, this.line.length);
    this.cursor = leading.length;
    this._refreshLine();
  }
};


Interface.prototype._deleteWordRight = function() {
  if (this.cursor < this.line.length) {
    var trailing = this.line.slice(this.cursor);
    var match = trailing.match(/^(?:\s+|\W+|\w+)\s*/);
    this.line = this.line.slice(0, this.cursor) +
                trailing.slice(match[0].length);
    this._refreshLine();
  }
};


Interface.prototype._deleteLineLeft = function() {
  this.line = this.line.slice(this.cursor);
  this.cursor = 0;
  this._refreshLine();
};


Interface.prototype._deleteLineRight = function() {
  this.line = this.line.slice(0, this.cursor);
  this._refreshLine();
};


Interface.prototype.clearLine = function() {
  this._moveCursor(+Infinity);
  this._writeToOutput('\r\n');
  this.line = '';
  this.cursor = 0;
  this.prevRows = 0;
};


Interface.prototype._line = function() {
  const line = this._addHistory();
  this.clearLine();
  this._onLine(line);
};


Interface.prototype._historyNext = function() {
  if (this.historyIndex > 0) {
    this.historyIndex--;
    this.line = this.history[this.historyIndex];
    this.cursor = this.line.length; // Set cursor to end of line.
    this._refreshLine();

  } else if (this.historyIndex === 0) {
    this.historyIndex = -1;
    this.cursor = 0;
    this.line = '';
    this._refreshLine();
  }
};


Interface.prototype._historyPrev = function() {
  if (this.historyIndex + 1 < this.history.length) {
    this.historyIndex++;
    this.line = this.history[this.historyIndex];
    this.cursor = this.line.length; // Set cursor to end of line.

    this._refreshLine();
  }
};


// Returns the last character's display position of the given string
Interface.prototype._getDisplayPos = function(str) {
  var offset = 0;
  const col = this.columns;
  var row = 0;
  var code;
  str = stripVTControlCharacters(str);
  for (var i = 0, len = str.length; i < len; i++) {
    code = str.codePointAt(i);
    if (code >= kUTF16SurrogateThreshold) { // Surrogates.
      i++;
    }
    if (code === 0x0a) { // new line \n
      // row must be incremented by 1 even if offset = 0 or col = +Infinity
      row += Math.ceil(offset / col) || 1;
      offset = 0;
      continue;
    }
    const width = getStringWidth(code);
    if (width === 0 || width === 1) {
      offset += width;
    } else { // width === 2
      if ((offset + 1) % col === 0) {
        offset++;
      }
      offset += 2;
    }
  }
  const cols = offset % col;
  const rows = row + (offset - cols) / col;
  return { cols: cols, rows: rows };
};


// Returns current cursor's position and line
Interface.prototype._getCursorPos = function() {
  const columns = this.columns;
  const strBeforeCursor = this._prompt + this.line.substring(0, this.cursor);
  const dispPos = this._getDisplayPos(
    stripVTControlCharacters(strBeforeCursor));
  var cols = dispPos.cols;
  var rows = dispPos.rows;
  // If the cursor is on a full-width character which steps over the line,
  // move the cursor to the beginning of the next line.
  if (cols + 1 === columns &&
      this.cursor < this.line.length &&
      isFullWidthCodePoint(this.line.codePointAt(this.cursor))) {
    rows++;
    cols = 0;
  }
  return { cols: cols, rows: rows };
};


// This function moves cursor dx places to the right
// (-dx for left) and refreshes the line if it is needed
Interface.prototype._moveCursor = function(dx) {
  const oldcursor = this.cursor;
  const oldPos = this._getCursorPos();
  this.cursor += dx;

  // bounds check
  if (this.cursor < 0) this.cursor = 0;
  else if (this.cursor > this.line.length) this.cursor = this.line.length;

  const newPos = this._getCursorPos();

  // Check if cursors are in the same line
  if (oldPos.rows === newPos.rows) {
    var diffCursor = this.cursor - oldcursor;
    var diffWidth;
    if (diffCursor < 0) {
      diffWidth = -getStringWidth(
        this.line.substring(this.cursor, oldcursor)
      );
    } else if (diffCursor > 0) {
      diffWidth = getStringWidth(
        this.line.substring(this.cursor, oldcursor)
      );
    }
    moveCursor(this.output, diffWidth, 0);
    this.prevRows = newPos.rows;
  } else {
    this._refreshLine();
  }
};

function _ttyWriteDumb(s, key) {
  key = key || {};

  if (key.name === 'escape') return;

  if (this._sawReturnAt && key.name !== 'enter')
    this._sawReturnAt = 0;

  if (key.ctrl) {
    if (key.name === 'c') {
      if (this.listenerCount('SIGINT') > 0) {
        this.emit('SIGINT');
      } else {
        // This readline instance is finished
        this.close();
      }

      return;
    } else if (key.name === 'd') {
      this.close();
      return;
    }
  }

  switch (key.name) {
    case 'return':  // Carriage return, i.e. \r
      this._sawReturnAt = Date.now();
      this._line();
      break;

    case 'enter':
      // When key interval > crlfDelay
      if (this._sawReturnAt === 0 ||
          Date.now() - this._sawReturnAt > this.crlfDelay) {
        this._line();
      }
      this._sawReturnAt = 0;
      break;

    default:
      if (typeof s === 'string' && s) {
        this.line += s;
        this.cursor += s.length;
        this._writeToOutput(s);
      }
  }
}

// Handle a write from the tty
Interface.prototype._ttyWrite = function(s, key) {
  const previousKey = this._previousKey;
  key = key || {};
  this._previousKey = key;

  // Ignore escape key, fixes
  // https://github.com/nodejs/node-v0.x-archive/issues/2876.
  if (key.name === 'escape') return;

  if (key.ctrl && key.shift) {
    /* Control and shift pressed */
    switch (key.name) {
      case 'backspace':
        this._deleteLineLeft();
        break;

      case 'delete':
        this._deleteLineRight();
        break;
    }

  } else if (key.ctrl) {
    /* Control key pressed */

    switch (key.name) {
      case 'c':
        if (this.listenerCount('SIGINT') > 0) {
          this.emit('SIGINT');
        } else {
          // This readline instance is finished
          this.close();
        }
        break;

      case 'h': // delete left
        this._deleteLeft();
        break;

      case 'd': // delete right or EOF
        if (this.cursor === 0 && this.line.length === 0) {
          // This readline instance is finished
          this.close();
        } else if (this.cursor < this.line.length) {
          this._deleteRight();
        }
        break;

      case 'u': // Delete from current to start of line
        this._deleteLineLeft();
        break;

      case 'k': // Delete from current to end of line
        this._deleteLineRight();
        break;

      case 'a': // Go to the start of the line
        this._moveCursor(-Infinity);
        break;

      case 'e': // Go to the end of the line
        this._moveCursor(+Infinity);
        break;

      case 'b': // back one character
        this._moveCursor(-charLengthLeft(this.line, this.cursor));
        break;

      case 'f': // Forward one character
        this._moveCursor(+charLengthAt(this.line, this.cursor));
        break;

      case 'l': // Clear the whole screen
        cursorTo(this.output, 0, 0);
        clearScreenDown(this.output);
        this._refreshLine();
        break;

      case 'n': // next history item
        this._historyNext();
        break;

      case 'p': // Previous history item
        this._historyPrev();
        break;

      case 'z':
        if (process.platform === 'win32') break;
        if (this.listenerCount('SIGTSTP') > 0) {
          this.emit('SIGTSTP');
        } else {
          process.once('SIGCONT', () => {
            // Don't raise events if stream has already been abandoned.
            if (!this.paused) {
              // Stream must be paused and resumed after SIGCONT to catch
              // SIGINT, SIGTSTP, and EOF.
              this.pause();
              this.emit('SIGCONT');
            }
            // Explicitly re-enable "raw mode" and move the cursor to
            // the correct position.
            // See https://github.com/joyent/node/issues/3295.
            this._setRawMode(true);
            this._refreshLine();
          });
          this._setRawMode(false);
          process.kill(process.pid, 'SIGTSTP');
        }
        break;

      case 'w': // Delete backwards to a word boundary
      case 'backspace':
        this._deleteWordLeft();
        break;

      case 'delete': // Delete forward to a word boundary
        this._deleteWordRight();
        break;

      case 'left':
        this._wordLeft();
        break;

      case 'right':
        this._wordRight();
        break;
    }

  } else if (key.meta) {
    /* Meta key pressed */

    switch (key.name) {
      case 'b': // backward word
        this._wordLeft();
        break;

      case 'f': // forward word
        this._wordRight();
        break;

      case 'd': // delete forward word
      case 'delete':
        this._deleteWordRight();
        break;

      case 'backspace': // Delete backwards to a word boundary
        this._deleteWordLeft();
        break;
    }

  } else {
    /* No modifier keys used */

    // \r bookkeeping is only relevant if a \n comes right after.
    if (this._sawReturnAt && key.name !== 'enter')
      this._sawReturnAt = 0;

    switch (key.name) {
      case 'return':  // Carriage return, i.e. \r
        this._sawReturnAt = Date.now();
        this._line();
        break;

      case 'enter':
        // When key interval > crlfDelay
        if (this._sawReturnAt === 0 ||
            Date.now() - this._sawReturnAt > this.crlfDelay) {
          this._line();
        }
        this._sawReturnAt = 0;
        break;

      case 'backspace':
        this._deleteLeft();
        break;

      case 'delete':
        this._deleteRight();
        break;

      case 'left':
        // Obtain the code point to the left
        this._moveCursor(-charLengthLeft(this.line, this.cursor));
        break;

      case 'right':
        this._moveCursor(+charLengthAt(this.line, this.cursor));
        break;

      case 'home':
        this._moveCursor(-Infinity);
        break;

      case 'end':
        this._moveCursor(+Infinity);
        break;

      case 'up':
        this._historyPrev();
        break;

      case 'down':
        this._historyNext();
        break;

      case 'tab':
        // If tab completion enabled, do that...
        if (typeof this.completer === 'function' && this.isCompletionEnabled) {
          const lastKeypressWasTab = previousKey && previousKey.name === 'tab';
          this._tabComplete(lastKeypressWasTab);
          break;
        }
        // falls through

      default:
        if (typeof s === 'string' && s) {
          var lines = s.split(/\r\n|\n|\r/);
          for (var i = 0, len = lines.length; i < len; i++) {
            if (i > 0) {
              this._line();
            }
            this._insertString(lines[i]);
          }
        }
    }
  }
};

Interface.prototype[Symbol.asyncIterator] = function() {
  if (this[kLineObjectStream] === undefined) {
    if (Readable === undefined) {
      Readable = require('stream').Readable;
    }
    const readable = new Readable({
      objectMode: true,
      read: () => {
        this.resume();
      },
      destroy: (err, cb) => {
        this.off('line', lineListener);
        this.off('close', closeListener);
        this.close();
        cb(err);
      }
    });
    const lineListener = (input) => {
      if (!readable.push(input)) {
        this.pause();
      }
    };
    const closeListener = () => {
      readable.push(null);
    };
    this.on('line', lineListener);
    this.on('close', closeListener);
    this[kLineObjectStream] = readable;
  }

  return this[kLineObjectStream][Symbol.asyncIterator]();
};

/**
 * accepts a readable Stream instance and makes it emit "keypress" events
 */

function emitKeypressEvents(stream, iface) {
  if (stream[KEYPRESS_DECODER]) return;

  if (StringDecoder === undefined)
    StringDecoder = require('string_decoder').StringDecoder;
  stream[KEYPRESS_DECODER] = new StringDecoder('utf8');

  stream[ESCAPE_DECODER] = emitKeys(stream);
  stream[ESCAPE_DECODER].next();

  const escapeCodeTimeout = () => stream[ESCAPE_DECODER].next('');
  let timeoutId;

  function onData(b) {
    if (stream.listenerCount('keypress') > 0) {
      var r = stream[KEYPRESS_DECODER].write(b);
      if (r) {
        clearTimeout(timeoutId);

        if (iface) {
          iface._sawKeyPress = r.length === 1;
        }

        for (var i = 0; i < r.length; i++) {
          if (r[i] === '\t' && typeof r[i + 1] === 'string' && iface) {
            iface.isCompletionEnabled = false;
          }

          try {
            stream[ESCAPE_DECODER].next(r[i]);
            // Escape letter at the tail position
            if (r[i] === kEscape && i + 1 === r.length) {
              timeoutId = setTimeout(
                escapeCodeTimeout,
                iface ? iface.escapeCodeTimeout : ESCAPE_CODE_TIMEOUT
              );
            }
          } catch (err) {
            // If the generator throws (it could happen in the `keypress`
            // event), we need to restart it.
            stream[ESCAPE_DECODER] = emitKeys(stream);
            stream[ESCAPE_DECODER].next();
            throw err;
          } finally {
            if (iface) {
              iface.isCompletionEnabled = true;
            }
          }
        }
      }
    } else {
      // Nobody's watching anyway
      stream.removeListener('data', onData);
      stream.on('newListener', onNewListener);
    }
  }

  function onNewListener(event) {
    if (event === 'keypress') {
      stream.on('data', onData);
      stream.removeListener('newListener', onNewListener);
    }
  }

  if (stream.listenerCount('keypress') > 0) {
    stream.on('data', onData);
  } else {
    stream.on('newListener', onNewListener);
  }
}

/**
 * moves the cursor to the x and y coordinate on the given stream
 */

function cursorTo(stream, x, y, callback) {
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  if (typeof y === 'function') {
    callback = y;
    y = undefined;
  }

  if (stream == null || (typeof x !== 'number' && typeof y !== 'number')) {
    if (typeof callback === 'function')
      process.nextTick(callback);
    return true;
  }

  if (typeof x !== 'number')
    throw new ERR_INVALID_CURSOR_POS();

  const data = typeof y !== 'number' ? CSI`${x + 1}G` : CSI`${y + 1};${x + 1}H`;
  return stream.write(data, callback);
}

/**
 * moves the cursor relative to its current location
 */

function moveCursor(stream, dx, dy, callback) {
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  if (stream == null || !(dx || dy)) {
    if (typeof callback === 'function')
      process.nextTick(callback);
    return true;
  }

  let data = '';

  if (dx < 0) {
    data += CSI`${-dx}D`;
  } else if (dx > 0) {
    data += CSI`${dx}C`;
  }

  if (dy < 0) {
    data += CSI`${-dy}A`;
  } else if (dy > 0) {
    data += CSI`${dy}B`;
  }

  return stream.write(data, callback);
}

/**
 * clears the current line the cursor is on:
 *   -1 for left of the cursor
 *   +1 for right of the cursor
 *    0 for the entire line
 */

function clearLine(stream, dir, callback) {
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  if (stream === null || stream === undefined) {
    if (typeof callback === 'function')
      process.nextTick(callback);
    return true;
  }

  const type = dir < 0 ? kClearToBeginning : dir > 0 ? kClearToEnd : kClearLine;
  return stream.write(type, callback);
}

/**
 * clears the screen from the current position of the cursor down
 */

function clearScreenDown(stream, callback) {
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  if (stream === null || stream === undefined) {
    if (typeof callback === 'function')
      process.nextTick(callback);
    return true;
  }

  return stream.write(kClearScreenDown, callback);
}

module.exports = {
  Interface,
  clearLine,
  clearScreenDown,
  createInterface,
  cursorTo,
  emitKeypressEvents,
  moveCursor
};
