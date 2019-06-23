'use strict';
const { Math, Object } = primordials;
const EventEmitter = require('events');
const { ERR_INVALID_OPT_VALUE } = require('internal/errors').codes;
const {
  clearScreenDown,
  cursorTo,
  emitKeypressEvents,
  getStringWidth,
  isFullWidthCodePoint,
  kEscapeCodeTimeout,
  kUTF16SurrogateThreshold,
  moveCursor,
  stripVTControlCharacters
} = require('internal/readline/utils');
const { inspect } = require('internal/util/inspect');
const { validateString } = require('internal/validators');

const kLineObjectStream = Symbol('line object stream');
const kHistorySize = 30;
const kMincrlfDelay = 100;
// \r\n, \n, or \r followed by something other than \n.
const lineEndingRegEx = /\r?\n|\r(?!\n)/;

let StringDecoder;  // Lazy loaded.
let Readable; // Lazy loaded.


function Interface(input, output, completer, terminal) {
  if (!(this instanceof Interface))
    return new Interface(input, output, completer, terminal);

  if (StringDecoder === undefined)
    StringDecoder = require('string_decoder').StringDecoder;

  this._sawReturnAt = 0;
  this.isCompletionEnabled = true;
  this._sawKeyPress = false;
  this._previousKey = null;
  this.escapeCodeTimeout = kEscapeCodeTimeout;

  EventEmitter.call(this);
  let historySize;
  let removeHistoryDuplicates = false;
  let crlfDelay;
  let prompt = '> ';

  if (input && input.input) {
    // An options object was given.
    output = input.output;
    completer = input.completer;
    terminal = input.terminal;
    historySize = input.historySize;
    removeHistoryDuplicates = input.removeHistoryDuplicates;

    if (input.prompt !== undefined)
      prompt = input.prompt;

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

  if (completer !== undefined && typeof completer !== 'function')
    throw new ERR_INVALID_OPT_VALUE('completer', completer);

  if (historySize === undefined)
    historySize = kHistorySize;

  if (typeof historySize !== 'number' ||
      Number.isNaN(historySize) ||
      historySize < 0) {
    throw new ERR_INVALID_OPT_VALUE.RangeError('historySize', historySize);
  }

  // Backwards compat; check the isTTY prop of the output stream
  //  when `terminal` was not specified
  if (terminal === undefined && !(output === null || output === undefined))
    terminal = !!output.isTTY;

  const self = this;

  this.output = output;
  this.input = input;
  this.historySize = historySize;
  this.removeHistoryDuplicates = !!removeHistoryDuplicates;
  this.crlfDelay = crlfDelay ?
    Math.max(kMincrlfDelay, crlfDelay) : kMincrlfDelay;

  // Check arity, 2 - for async, 1 for sync.
  if (typeof completer === 'function') {
    this.completer = completer.length === 2 ?
      completer :
      function completerWrapper(v, cb) {
        cb(null, completer(v));
      };
  }

  this.setPrompt(prompt);

  this.terminal = !!terminal;

  if (process.env.TERM === 'dumb')
    this._ttyWrite = ttyWriteDumb.bind(this);

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
    if (typeof self.line === 'string' && self.line.length > 0)
      self.emit('line', self.line);

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

      if (output !== null && output !== undefined)
        output.removeListener('resize', onresize);
    }

    emitKeypressEvents(input, this);

    // `input` usually refers to stdin.
    input.on('keypress', onkeypress);
    input.on('end', ontermend);

    // The current line.
    this.line = '';

    this._setRawMode(true);
    this.terminal = true;

    // The cursor position on the line.
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
  get() {
    return this.output && this.output.columns ? this.output.columns : Infinity;
  }
});


Interface.prototype.setPrompt = function(prompt) {
  this._prompt = prompt;
};


Interface.prototype._setRawMode = function(mode) {
  const wasInRawMode = this.input.isRaw;

  if (typeof this.input.setRawMode === 'function')
    this.input.setRawMode(mode);

  return wasInRawMode;
};


Interface.prototype.prompt = function(preserveCursor) {
  if (this.paused)
    this.resume();

  if (this.terminal && process.env.TERM !== 'dumb') {
    if (!preserveCursor)
      this.cursor = 0;
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
    const cb = this._questionCallback;
    this._questionCallback = null;
    this.setPrompt(this._oldPrompt);
    cb(line);
  } else {
    this.emit('line', line);
  }
};


Interface.prototype._writeToOutput = function(stringToWrite) {
  validateString(stringToWrite, 'stringToWrite');

  if (this.output !== null && this.output !== undefined)
    this.output.write(stringToWrite);
};


Interface.prototype._addHistory = function() {
  if (this.line.length === 0)
    return '';

  // If the history is disabled then return the line.
  if (this.historySize === 0)
    return this.line;

  // If the trimmed line is empty then return the line.
  if (this.line.trim().length === 0)
    return this.line;

  if (this.history.length === 0 || this.history[0] !== this.line) {
    if (this.removeHistoryDuplicates) {
      // Remove the older history line if it is identical to new one.
      const dupIndex = this.history.indexOf(this.line);

      if (dupIndex !== -1)
        this.history.splice(dupIndex, 1);
    }

    this.history.unshift(this.line);

    // Only store as many entries as the history size allows.
    if (this.history.length > this.historySize)
      this.history.pop();
  }

  this.historyIndex = -1;
  return this.history[0];
};


Interface.prototype._refreshLine = function() {
  // The line length.
  const line = this._prompt + this.line;
  const dispPos = this._getDisplayPos(line);
  const lineCols = dispPos.cols;
  const lineRows = dispPos.rows;

  // The cursor position.
  const cursorPos = this._getCursorPos();

  // First, move to the bottom of the current line, based on cursor position.
  const prevRows = this.prevRows || 0;

  if (prevRows > 0)
    moveCursor(this.output, 0, -prevRows);

  // Move the cursor to the left edge.
  cursorTo(this.output, 0);

  // Erase data.
  clearScreenDown(this.output);

  // Write the prompt and the current buffer content.
  this._writeToOutput(line);

  // Force the terminal to allocate a new line.
  if (lineCols === 0)
    this._writeToOutput(' ');

  // Move the cursor to the original position.
  cursorTo(this.output, cursorPos.cols);

  const diff = lineRows - cursorPos.rows;

  if (diff > 0)
    moveCursor(this.output, 0, -diff);

  this.prevRows = cursorPos.rows;
};


Interface.prototype.close = function() {
  if (this.closed)
    return;

  this.pause();

  if (this.terminal)
    this._setRawMode(false);

  this.closed = true;
  this.emit('close');
};


Interface.prototype.pause = function() {
  if (this.paused)
    return;

  this.input.pause();
  this.paused = true;
  this.emit('pause');
  return this;
};


Interface.prototype.resume = function() {
  if (!this.paused)
    return;

  this.input.resume();
  this.paused = false;
  this.emit('resume');
  return this;
};


Interface.prototype.write = function(d, key) {
  if (this.paused)
    this.resume();

  if (this.terminal)
    this._ttyWrite(d, key);
  else
    this._normalWrite(d);
};


Interface.prototype._normalWrite = function(b) {
  if (b === undefined)
    return;

  let string = this._decoder.write(b);

  if (this._sawReturnAt && Date.now() - this._sawReturnAt <= this.crlfDelay) {
    string = string.replace(/^\n/, '');
    this._sawReturnAt = 0;
  }

  // Run test() on the new string chunk, not on the entire line buffer.
  const newPartContainsEnding = lineEndingRegEx.test(string);

  if (this._line_buffer) {
    string = this._line_buffer + string;
    this._line_buffer = null;
  }

  if (newPartContainsEnding) {
    this._sawReturnAt = string.endsWith('\r') ? Date.now() : 0;

    // Got one or more newlines. Translate them into "line" events.
    const lines = string.split(lineEndingRegEx);
    // Either '' or (conceivably) the unfinished portion of the next line.
    string = lines.pop();
    this._line_buffer = string;
    for (let n = 0; n < lines.length; n++)
      this._onLine(lines[n]);
  } else if (string) {
    // No newlines this time. Save the rest for next time.
    this._line_buffer = string;
  }
};


Interface.prototype._insertString = function(c) {
  if (this.cursor < this.line.length) {
    const beg = this.line.slice(0, this.cursor);
    const end = this.line.slice(this.cursor, this.line.length);

    this.line = beg + c + end;
    this.cursor += c.length;
    this._refreshLine();
  } else {
    this.line += c;
    this.cursor += c.length;

    if (this._getCursorPos().cols === 0)
      this._refreshLine();
    else
      this._writeToOutput(c);

    // A hack to get the line refreshed if it's needed.
    this._moveCursor(0);
  }
};


Interface.prototype._tabComplete = function(lastKeypressWasTab) {
  this.pause();
  this.completer(this.line.slice(0, this.cursor), (err, rv) => {
    onTabComplete(err, rv, this, lastKeypressWasTab);
  });
};


Interface.prototype._wordLeft = function() {
  if (this.cursor > 0) {
    // Reverse the string and match a word near the beginning in order to avoid
    // quadratic time complexity.
    const leading = this.line.slice(0, this.cursor);
    const reversed = leading.split('').reverse().join('');
    const match = reversed.match(/^\s*(?:[^\w\s]+|\w+)?/);
    this._moveCursor(-match[0].length);
  }
};


Interface.prototype._wordRight = function() {
  if (this.cursor < this.line.length) {
    const trailing = this.line.slice(this.cursor);
    const match = trailing.match(/^(?:\s+|[^\w\s]+|\w+)\s*/);
    this._moveCursor(match[0].length);
  }
};


Interface.prototype._deleteLeft = function() {
  if (this.cursor > 0 && this.line.length > 0) {
    // The number of UTF-16 units comprising the character to the left.
    const charSize = charLengthLeft(this.line, this.cursor);
    this.line = this.line.slice(0, this.cursor - charSize) +
                this.line.slice(this.cursor, this.line.length);

    this.cursor -= charSize;
    this._refreshLine();
  }
};


Interface.prototype._deleteRight = function() {
  if (this.cursor < this.line.length) {
    // The number of UTF-16 units comprising the character to the right.
    const charSize = charLengthAt(this.line, this.cursor);
    this.line = this.line.slice(0, this.cursor) +
                this.line.slice(this.cursor + charSize, this.line.length);
    this._refreshLine();
  }
};


Interface.prototype._deleteWordLeft = function() {
  if (this.cursor > 0) {
    // Reverse the string and match a word near the beginning in order to avoid
    // quadratic time complexity.
    let leading = this.line.slice(0, this.cursor);
    const reversed = leading.split('').reverse().join('');
    const match = reversed.match(/^\s*(?:[^\w\s]+|\w+)?/);
    leading = leading.slice(0, leading.length - match[0].length);
    this.line = leading + this.line.slice(this.cursor, this.line.length);
    this.cursor = leading.length;
    this._refreshLine();
  }
};


Interface.prototype._deleteWordRight = function() {
  if (this.cursor < this.line.length) {
    const trailing = this.line.slice(this.cursor);
    const match = trailing.match(/^(?:\s+|\W+|\w+)\s*/);
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
  this._moveCursor(Infinity);
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
    this.cursor = this.line.length; // Set the cursor to the end of the line.
    this._refreshLine();
  } else if (this.historyIndex === 0) {
    this.historyIndex = -1;
    this.line = '';
    this.cursor = 0;
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


// Returns the last character's display position of the given string.
Interface.prototype._getDisplayPos = function(str) {
  let offset = 0;
  const col = this.columns;
  let row = 0;

  str = stripVTControlCharacters(str);

  for (let i = 0, len = str.length; i < len; i++) {
    const code = str.codePointAt(i);

    if (code >= 0x10000)  // Surrogates.
      i++;

    if (code === 0x0a) {  // New line \n.
      // Row must be incremented by 1 even if offset = 0 or col = +Infinity.
      row += Math.ceil(offset / col) || 1;
      offset = 0;
      continue;
    }

    const width = getStringWidth(code);

    if (width === 0 || width === 1) {
      offset += width;
    } else { // Width equals two.
      if ((offset + 1) % col === 0)
        offset++;

      offset += 2;
    }
  }

  const cols = offset % col;
  const rows = row + (offset - cols) / col;
  return { cols, rows };
};


// Returns the current cursor's position and line.
Interface.prototype._getCursorPos = function() {
  const columns = this.columns;
  const strBeforeCursor = this._prompt + this.line.substring(0, this.cursor);
  const dispPos = this._getDisplayPos(
    stripVTControlCharacters(strBeforeCursor));
  let cols = dispPos.cols;
  let rows = dispPos.rows;
  // If the cursor is on a full-width character which steps over the line,
  // move the cursor to the beginning of the next line.
  if (cols + 1 === columns &&
      this.cursor < this.line.length &&
      isFullWidthCodePoint(this.line.codePointAt(this.cursor))) {
    rows++;
    cols = 0;
  }
  return { cols, rows };
};


// This function moves cursor dx places to the right
// (-dx for left) and refreshes the line if it is needed.
Interface.prototype._moveCursor = function(dx) {
  const oldcursor = this.cursor;
  const oldPos = this._getCursorPos();
  this.cursor += dx;

  // Bounds check.
  if (this.cursor < 0)
    this.cursor = 0;
  else if (this.cursor > this.line.length)
    this.cursor = this.line.length;

  const newPos = this._getCursorPos();

  // Check if cursors are in the same row.
  if (oldPos.rows === newPos.rows) {
    const diffCursor = this.cursor - oldcursor;
    let diffWidth;

    if (diffCursor < 0)
      diffWidth = -getStringWidth(this.line.substring(this.cursor, oldcursor));
    else if (diffCursor > 0)
      diffWidth = getStringWidth(this.line.substring(this.cursor, oldcursor));

    moveCursor(this.output, diffWidth, 0);
    this.prevRows = newPos.rows;
  } else {
    this._refreshLine();
  }
};


Interface.prototype[Symbol.asyncIterator] = function() {
  if (this[kLineObjectStream] === undefined) {
    if (Readable === undefined)
      Readable = require('stream').Readable;

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
      if (!readable.push(input))
        this.pause();
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


Interface.prototype._ttyWrite = function(s, key) {
  // Handle a write from the tty.
  const previousKey = this._previousKey;
  key = key || {};
  this._previousKey = key;

  // Ignore escape key, fixes
  // https://github.com/nodejs/node-v0.x-archive/issues/2876.
  if (key.name === 'escape')
    return;

  if (key.ctrl && key.shift) {
    // Control key and Shift pressed.
    switch (key.name) {
      case 'backspace':
        this._deleteLineLeft();
        break;
      case 'delete':
        this._deleteLineRight();
        break;
    }
  } else if (key.ctrl) {
    // Control key pressed.
    switch (key.name) {
      case 'c':
        if (this.listenerCount('SIGINT') > 0)
          this.emit('SIGINT');
        else
          this.close(); // This readline instance is finished.
        break;
      case 'h': // Delete left.
        this._deleteLeft();
        break;
      case 'd': // Delete right or EOF.
        if (this.cursor === 0 && this.line.length === 0)
          this.close(); // This readline instance is finished.
        else if (this.cursor < this.line.length)
          this._deleteRight();
        break;
      case 'u': // Delete from the current position to the start of the line.
        this._deleteLineLeft();
        break;
      case 'k': // Delete from the current position to the end of the line.
        this._deleteLineRight();
        break;
      case 'a': // Go to the start of the line.
        this._moveCursor(-Infinity);
        break;
      case 'e': // Go to the end of the line.
        this._moveCursor(Infinity);
        break;
      case 'b': // Back one character.
        this._moveCursor(-charLengthLeft(this.line, this.cursor));
        break;
      case 'f': // Forward one character.
        this._moveCursor(charLengthAt(this.line, this.cursor));
        break;
      case 'l': // Clear the whole screen.
        cursorTo(this.output, 0, 0);
        clearScreenDown(this.output);
        this._refreshLine();
        break;
      case 'n': // Next history item.
        this._historyNext();
        break;
      case 'p': // Previous history item.
        this._historyPrev();
        break;
      case 'z':
        if (process.platform === 'win32')
          break;

        if (this.listenerCount('SIGTSTP') > 0) {
          this.emit('SIGTSTP');
        } else {
          process.once('SIGCONT', (function continueProcess(self) {
            return function() {
              // Don't raise events if the stream has already been abandoned.
              if (!self.paused) {
                // Stream must be paused and resumed after SIGCONT to catch
                // SIGINT, SIGTSTP, and EOF.
                self.pause();
                self.emit('SIGCONT');
              }
              // Explicitly re-enable "raw mode" and move the cursor to
              // the correct position.
              // See https://github.com/joyent/node/issues/3295.
              self._setRawMode(true);
              self._refreshLine();
            };
          })(this));
          this._setRawMode(false);
          process.kill(process.pid, 'SIGTSTP');
        }
        break;
      case 'w': // Delete backwards to a word boundary.
      case 'backspace':
        this._deleteWordLeft();
        break;
      case 'delete': // Delete forward to a word boundary.
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
    // Meta key pressed.
    switch (key.name) {
      case 'b': // Backward word.
        this._wordLeft();
        break;
      case 'f': // Forward word.
        this._wordRight();
        break;
      case 'd': // Delete forward word.
      case 'delete':
        this._deleteWordRight();
        break;
      case 'backspace': // Delete backwards to a word boundary.
        this._deleteWordLeft();
        break;
    }
  } else {
    // No modifier keys used.

    // \r bookkeeping is only relevant if a \n comes right after.
    if (this._sawReturnAt && key.name !== 'enter')
      this._sawReturnAt = 0;

    switch (key.name) {
      case 'return':  // Carriage return, i.e. \r.
        this._sawReturnAt = Date.now();
        this._line();
        break;
      case 'enter':
        // When key interval > crlfDelay.
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
        // Obtain the code point to the left.
        this._moveCursor(-charLengthLeft(this.line, this.cursor));
        break;
      case 'right':
        this._moveCursor(charLengthAt(this.line, this.cursor));
        break;
      case 'home':
        this._moveCursor(-Infinity);
        break;
      case 'end':
        this._moveCursor(Infinity);
        break;
      case 'up':
        this._historyPrev();
        break;
      case 'down':
        this._historyNext();
        break;
      case 'tab':
        // If tab completion is enabled, perform tab completion.
        if (typeof this.completer === 'function' && this.isCompletionEnabled) {
          const lastKeypressWasTab = previousKey && previousKey.name === 'tab';
          this._tabComplete(lastKeypressWasTab);
          break;
        }
        // Fall through.
      default:
        if (typeof s === 'string' && s.length > 0) {
          const lines = s.split(/\r\n|\n|\r/);

          for (let i = 0, len = lines.length; i < len; i++) {
            if (i > 0)
              this._line();

            this._insertString(lines[i]);
          }
        }
    }
  }
};


function createInterface(input, output, completer, terminal) {
  return new Interface(input, output, completer, terminal);
}


function handleGroup(self, group, width, maxColumns) {
  if (group.length === 0)
    return;

  const minRows = Math.ceil(group.length / maxColumns);

  for (let row = 0; row < minRows; row++) {
    for (let col = 0; col < maxColumns; col++) {
      const idx = row * maxColumns + col;

      if (idx >= group.length)
        break;

      const item = group[idx];
      self._writeToOutput(item);
      if (col < maxColumns - 1) {
        for (let s = 0; s < width - item.length; s++)
          self._writeToOutput(' ');
      }
    }

    self._writeToOutput('\r\n');
  }

  self._writeToOutput('\r\n');
}


function commonPrefix(strings) {
  if (strings.length === 0)
    return '';

  if (strings.length === 1)
    return strings[0];

  const sorted = strings.slice().sort();
  const min = sorted[0];
  const max = sorted[sorted.length - 1];

  for (let i = 0, len = min.length; i < len; i++) {
    if (min[i] !== max[i])
      return min.slice(0, i);
  }

  return min;
}


function ttyWriteDumb(s, key) {
  key = key || {};

  if (key.name === 'escape')
    return;

  if (this._sawReturnAt && key.name !== 'enter')
    this._sawReturnAt = 0;

  if (key.ctrl && key.name === 'c') {
    if (this.listenerCount('SIGINT') > 0)
      this.emit('SIGINT');
    else
      this.close(); // The readline instance is finished.
  }

  switch (key.name) {
    case 'return':  // Carriage return, i.e. \r.
      this._sawReturnAt = Date.now();
      this._line();
      break;
    case 'enter':
      // When key interval > crlfDelay.
      if (this._sawReturnAt === 0 ||
          Date.now() - this._sawReturnAt > this.crlfDelay) {
        this._line();
      }
      this._sawReturnAt = 0;
      break;
    default:
      if (typeof s === 'string' && s.length !== 0) {
        this.line += s;
        this.cursor += s.length;
        this._writeToOutput(s);
      }
  }
}


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


function onTabComplete(err, completionResult, rlInterface, lastKeypressWasTab) {
  rlInterface.resume();

  if (err) {
    rlInterface._writeToOutput(`tab completion error ${inspect(err)}`);
    return;
  }

  const completions = completionResult[0];
  const completeOn = completionResult[1];  // The text that was completed.

  if (completions && completions.length) {
    // Apply/show completions.
    if (lastKeypressWasTab) {
      rlInterface._writeToOutput('\r\n');
      const width = completions.reduce(function completionReducer(a, b) {
        return a.length > b.length ? a : b;
      }).length + 2;  // Two space padding.
      let maxColumns = Math.floor(rlInterface.columns / width);

      if (!maxColumns || maxColumns === Infinity)
        maxColumns = 1;

      let group = [];

      for (let i = 0; i < completions.length; i++) {
        const c = completions[i];

        if (c === '') {
          handleGroup(rlInterface, group, width, maxColumns);
          group = [];
        } else {
          group.push(c);
        }
      }

      handleGroup(rlInterface, group, width, maxColumns);
    }

    // If there is a common prefix to all matches, then apply that portion.
    const f = completions.filter((e) => e);
    const prefix = commonPrefix(f);

    if (prefix.length > completeOn.length)
      rlInterface._insertString(prefix.slice(completeOn.length));

    rlInterface._refreshLine();
  }
}


module.exports = { createInterface, Interface };
