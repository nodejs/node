'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypeShift,
  ArrayPrototypeSplice,
  ArrayPrototypeUnshift,
  Boolean,
  RegExpPrototypeSymbolSplit,
  StringPrototypeStartsWith,
  StringPrototypeTrim,
  Symbol,
} = primordials;

const { validateNumber, validateArray } = require('internal/validators');

const path = require('path');
const fs = require('fs');
const os = require('os');
let debug = require('internal/util/debuglog').debuglog('repl', (fn) => {
  debug = fn;
});
const permission = require('internal/process/permission');
const { clearTimeout, setTimeout } = require('timers');
const {
  reverseString,
} = require('internal/readline/utils');

// The debounce is to guard against code pasted into the REPL.
const kDebounceHistoryMS = 15;
const kHistorySize = 30;

// Class fields
const kTimer = Symbol('_kTimer');
const kWriting = Symbol('_kWriting');
const kPending = Symbol('_kPending');
const kRemoveHistoryDuplicates = Symbol('_kRemoveHistoryDuplicates');
const kHistoryHandle = Symbol('_kHistoryHandle');
const kHistoryPath = Symbol('_kHistoryPath');
const kContext = Symbol('_kContext');
const kIsFlushing = Symbol('_kIsFlushing');
const kHistory = Symbol('_kHistory');
const kSize = Symbol('_kSize');
const kIndex = Symbol('_kIndex');

// Class methods
const kNormalizeLineEndings = Symbol('_kNormalizeLineEndings');
const kWriteToOutput = Symbol('_kWriteToOutput');
const kOnLine = Symbol('_kOnLine');
const kOnExit = Symbol('_kOnExit');
const kInitializeHistory = Symbol('_kInitializeHistory');
const kHandleHistoryInitError = Symbol('_kHandleHistoryInitError');
const kHasWritePermission = Symbol('_kHasWritePermission');
const kValidateOptions = Symbol('_kValidateOptions');
const kResolveHistoryPath = Symbol('_kResolveHistoryPath');
const kReplHistoryMessage = Symbol('_kReplHistoryMessage');
const kFlushHistory = Symbol('_kFlushHistory');
const kGetHistoryPath = Symbol('_kGetHistoryPath');

class ReplHistory {
  constructor(context, options) {
    this[kValidateOptions](options);

    this[kHistoryPath] = ReplHistory[kGetHistoryPath](options);
    this[kContext] = context;
    this[kTimer] = null;
    this[kWriting] = false;
    this[kPending] = false;
    this[kRemoveHistoryDuplicates] = options.removeHistoryDuplicates || false;
    this[kHistoryHandle] = null;
    this[kIsFlushing] = false;
    this[kSize] = options.size ?? context.historySize ?? kHistorySize;
    this[kHistory] = options.history ?? [];
    this[kIndex] = -1;
  }

  initialize(onReadyCallback) {
    // Empty string disables persistent history
    if (this[kHistoryPath] === '') {
      // Save a reference to the context's original _historyPrev
      this.historyPrev = this[kContext]._historyPrev;
      this[kContext]._historyPrev = this[kReplHistoryMessage].bind(this);
      return onReadyCallback(null, this[kContext]);
    }

    const resolvedPath = this[kResolveHistoryPath]();
    if (!resolvedPath) {
      ReplHistory[kWriteToOutput](
        this[kContext],
        '\nError: Could not get the home directory.\n' +
        'REPL session history will not be persisted.\n',
      );

      // Save a reference to the context's original _historyPrev
      this.historyPrev = this[kContext]._historyPrev;
      this[kContext]._historyPrev = this[kReplHistoryMessage].bind(this);
      return onReadyCallback(null, this[kContext]);
    }

    if (!this[kHasWritePermission]()) {
      ReplHistory[kWriteToOutput](
        this[kContext],
        '\nAccess to FileSystemWrite is restricted.\n' +
        'REPL session history will not be persisted.\n',
      );
      return onReadyCallback(null, this[kContext]);
    }

    this[kContext].pause();

    this[kInitializeHistory](onReadyCallback).catch((err) => {
      this[kHandleHistoryInitError](err, onReadyCallback);
    });
  }

  addHistory(isMultiline, lastCommandErrored) {
    const line = this[kContext].line;

    if (line.length === 0) return '';

    // If the history is disabled then return the line
    if (this[kSize] === 0) return line;

    // If the trimmed line is empty then return the line
    if (StringPrototypeTrim(line).length === 0) return line;

    // This is necessary because each line would be saved in the history while creating
    // a new multiline, and we don't want that.
    if (isMultiline && this[kIndex] === -1) {
      ArrayPrototypeShift(this[kHistory]);
    } else if (lastCommandErrored) {
      // If the last command errored and we are trying to edit the history to fix it
      // remove the broken one from the history
      ArrayPrototypeShift(this[kHistory]);
    }

    const normalizedLine = ReplHistory[kNormalizeLineEndings](line, '\n', '\r');

    if (this[kHistory].length === 0 || this[kHistory][0] !== normalizedLine) {
      if (this[kRemoveHistoryDuplicates]) {
        // Remove older history line if identical to new one
        const dupIndex = ArrayPrototypeIndexOf(this[kHistory], line);
        if (dupIndex !== -1) ArrayPrototypeSplice(this[kHistory], dupIndex, 1);
      }

      // Add the new line to the history
      ArrayPrototypeUnshift(this[kHistory], normalizedLine);

      // Only store so many
      if (this[kHistory].length > this[kSize])
        ArrayPrototypePop(this[kHistory]);
    }

    this[kIndex] = -1;

    const finalLine = isMultiline ? reverseString(this[kHistory][0]) : this[kHistory][0];

    // The listener could change the history object, possibly
    // to remove the last added entry if it is sensitive and should
    // not be persisted in the history, like a password
    // Emit history event to notify listeners of update
    this[kContext].emit('history', this[kHistory]);

    return finalLine;
  }

  canNavigateToNext() {
    return this[kIndex] > -1 && this[kHistory].length > 0;
  }

  navigateToNext(substringSearch) {
    if (!this.canNavigateToNext()) {
      return null;
    }
    const search = substringSearch || '';
    let index = this[kIndex] - 1;

    while (
      index >= 0 &&
      (!StringPrototypeStartsWith(this[kHistory][index], search) ||
        this[kContext].line === this[kHistory][index])
    ) {
      index--;
    }

    this[kIndex] = index;

    if (index === -1) {
      return search;
    }

    return ReplHistory[kNormalizeLineEndings](this[kHistory][index], '\r', '\n');
  }

  canNavigateToPrevious() {
    return this[kHistory].length !== this[kIndex] && this[kHistory].length > 0;
  }

  navigateToPrevious(substringSearch = '') {
    if (!this.canNavigateToPrevious()) {
      return null;
    }
    const search = substringSearch || '';
    let index = this[kIndex] + 1;

    while (
      index < this[kHistory].length &&
      (!StringPrototypeStartsWith(this[kHistory][index], search) ||
        this[kContext].line === this[kHistory][index])
    ) {
      index++;
    }

    this[kIndex] = index;

    if (index === this[kHistory].length) {
      return search;
    }

    return ReplHistory[kNormalizeLineEndings](this[kHistory][index], '\r', '\n');
  }

  get size() { return this[kSize]; }
  get isFlushing() { return this[kIsFlushing]; }
  get history() { return this[kHistory]; }
  set history(value) { this[kHistory] = value; }
  get index() { return this[kIndex]; }
  set index(value) { this[kIndex] = value; }

  // Start private methods

  static [kGetHistoryPath](options) {
    let historyPath = options.filePath;
    if (typeof historyPath === 'string') {
      historyPath = StringPrototypeTrim(historyPath);
    }
    return historyPath;
  }

  static [kNormalizeLineEndings](line, from, to) {
    // Multiline history entries are saved reversed
    // History is structured with the newest entries at the top
    // and the oldest at the bottom. Multiline histories, however, only occupy
    // one line in the history file. When loading multiline history with
    // an old node binary, the history will be saved in the old format.
    // This is why we need to reverse the multilines.
    // Reversing the multilines is necessary when adding / editing and displaying them
    return reverseString(line, from, to);
  }

  static [kWriteToOutput](context, message) {
    if (typeof context._writeToOutput === 'function') {
      context._writeToOutput(message);
      if (typeof context._refreshLine === 'function') {
        context._refreshLine();
      }
    }
  }

  [kResolveHistoryPath]() {
    if (!this[kHistoryPath]) {
      try {
        const homedir = os.homedir();
        if (process.platform === 'win32') {
          // On Windows, the history file is stored in the Node.js app data directory
          // https://learn.microsoft.com/en-us/dotnet/api/system.environment.specialfolder?view=net-6.0
          const appDataPath = process.env.APPDATA || path.join(homedir, 'AppData', 'Roaming');
          const appDataNodePath = path.join(appDataPath, 'Node.js');
          if (!fs.existsSync(appDataNodePath)) {
            fs.mkdirSync(appDataNodePath, { recursive: true });
          }
          this[kHistoryPath] = path.join(appDataNodePath, '.node_repl_history');
        } else {
          this[kHistoryPath] = path.join(homedir, '.node_repl_history');
        }
        return this[kHistoryPath];
      } catch (err) {
        debug(err.stack);
        return null;
      }
    }
    return this[kHistoryPath];
  }

  [kHasWritePermission]() {
    return !(permission.isEnabled() &&
             permission.has('fs.write', this[kHistoryPath]) === false);
  }

  [kValidateOptions](options) {
    if (typeof options.history !== 'undefined') {
      validateArray(options.history, 'history');
    }
    if (typeof options.size !== 'undefined') {
      validateNumber(options.size, 'size', 0);
    }
  }

  async [kInitializeHistory](onReadyCallback) {
    try {
      // Open and close file first to ensure it exists
      // History files are conventionally not readable by others
      // 0o0600 = read/write for owner only
      const hnd = await fs.promises.open(this[kHistoryPath], 'a+', 0o0600);
      await hnd.close();

      let data;
      try {
        data = await fs.promises.readFile(this[kHistoryPath], 'utf8');
      } catch (err) {
        return this[kHandleHistoryInitError](err, onReadyCallback);
      }

      if (data) {
        this[kHistory] = RegExpPrototypeSymbolSplit(/\r?\n+/, data, this[kSize]);
      } else {
        this[kHistory] = [];
      }

      validateArray(this[kHistory], 'history');

      const handle = await fs.promises.open(this[kHistoryPath], 'r+');
      this[kHistoryHandle] = handle;

      await handle.truncate(0);

      this[kContext].on('line', this[kOnLine].bind(this));
      this[kContext].once('exit', this[kOnExit].bind(this));

      this[kContext].once('flushHistory', () => {
        if (!this[kContext].closed) {
          this[kContext].resume();
          onReadyCallback(null, this[kContext]);
        }
      });

      await this[kFlushHistory]();
    } catch (err) {
      return this[kHandleHistoryInitError](err, onReadyCallback);
    }
  }

  [kHandleHistoryInitError](err, onReadyCallback) {
    // Cannot open history file.
    // Don't crash, just don't persist history.
    ReplHistory[kWriteToOutput](
      this[kContext],
      '\nError: Could not open history file.\n' +
      'REPL session history will not be persisted.\n',
    );
    debug(err.stack);

    // Save a reference to the context's original _historyPrev
    this.historyPrev = this[kContext]._historyPrev;
    this[kContext]._historyPrev = this[kReplHistoryMessage].bind(this);
    this[kContext].resume();
    return onReadyCallback(null, this[kContext]);
  }

  [kOnLine]() {
    this[kIsFlushing] = true;

    if (this[kTimer]) {
      clearTimeout(this[kTimer]);
    }

    this[kTimer] = setTimeout(() => this[kFlushHistory](), kDebounceHistoryMS);
  }

  async [kFlushHistory]() {
    this[kTimer] = null;
    if (this[kWriting]) {
      this[kPending] = true;
      return;
    }

    this[kWriting] = true;
    const historyData = ArrayPrototypeJoin(this[kHistory], '\n');

    try {
      await this[kHistoryHandle].write(historyData, 0, 'utf8');
      this[kWriting] = false;

      if (this[kPending]) {
        this[kPending] = false;
        this[kOnLine]();
      } else {
        this[kIsFlushing] = Boolean(this[kTimer]);
        if (!this[kIsFlushing]) {
          this[kContext].emit('flushHistory');
        }
      }
    } catch (err) {
      this[kWriting] = false;
      debug('Error writing history file:', err);
    }
  }

  async [kOnExit]() {
    if (this[kIsFlushing]) {
      this[kContext].once('flushHistory', this[kOnExit].bind(this));
      return;
    }
    this[kContext].off('line', this[kOnLine].bind(this));

    if (this[kHistoryHandle] !== null) {
      try {
        await this[kHistoryHandle].close();
      } catch (err) {
        debug('Error closing history file:', err);
      }
    }
  }

  [kReplHistoryMessage]() {
    if (this[kHistory].length === 0) {
      ReplHistory[kWriteToOutput](
        this[kContext],
        '\nPersistent history support disabled. ' +
        'Set the NODE_REPL_HISTORY environment\nvariable to ' +
        'a valid, user-writable path to enable.\n',
      );
    }
    // First restore the original method on the context
    this[kContext]._historyPrev = this.historyPrev;
    // Then call it with the correct context
    return this[kContext]._historyPrev();
  }
}

module.exports = {
  ReplHistory,
};
