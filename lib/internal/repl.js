'use strict';

const Interface = require('readline').Interface;
const REPL = require('repl');
const path = require('path');
const fs = require('fs');
const os = require('os');
const util = require('util');
const debug = util.debuglog('repl');

module.exports = Object.create(REPL);
module.exports.createInternalRepl = createRepl;

// XXX(chrisdickinson): The 15ms debounce value is somewhat arbitrary.
// The debounce is to guard against code pasted into the REPL.
const kDebounceHistoryMS = 15;

function _writeToOutput(repl, message) {
  repl._writeToOutput(message);
  repl._refreshLine();
}

function createRepl(env, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts;
    opts = null;
  }
  opts = util._extend({
    ignoreUndefined: false,
    terminal: process.stdout.isTTY,
    useGlobal: true,
    breakEvalOnSigint: true
  }, opts);

  if (parseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }
  // the "dumb" special terminal, as defined by terminfo, doesn't support
  // ANSI colour control codes.
  // see http://invisible-island.net/ncurses/terminfo.ti.html#toc-_Specials
  if (parseInt(env.NODE_DISABLE_COLORS) || env.TERM === 'dumb') {
    opts.useColors = false;
  }

  opts.replMode = {
    'strict': REPL.REPL_MODE_STRICT,
    'sloppy': REPL.REPL_MODE_SLOPPY
  }[String(env.NODE_REPL_MODE).toLowerCase().trim()];

  if (opts.replMode === undefined) {
    opts.replMode = REPL.REPL_MODE_SLOPPY;
  }

  const historySize = Number(env.NODE_REPL_HISTORY_SIZE);
  if (!isNaN(historySize) && historySize > 0) {
    opts.historySize = historySize;
  } else {
    // XXX(chrisdickinson): set here to avoid affecting existing applications
    // using repl instances.
    opts.historySize = 1000;
  }

  const repl = REPL.start(opts);
  if (opts.terminal) {
    return setupHistory(repl, env.NODE_REPL_HISTORY,
                        env.NODE_REPL_HISTORY_FILE, cb);
  }

  repl._historyPrev = _replHistoryMessage;
  cb(null, repl);
}

function setupHistory(repl, historyPath, oldHistoryPath, ready) {
  // Empty string disables persistent history.

  if (typeof historyPath === 'string')
    historyPath = historyPath.trim();

  if (historyPath === '') {
    repl._historyPrev = _replHistoryMessage;
    return ready(null, repl);
  }

  if (!historyPath) {
    try {
      historyPath = path.join(os.homedir(), '.node_repl_history');
    } catch (err) {
      _writeToOutput(repl, '\nError: Could not get the home directory.\n' +
                     'REPL session history will not be persisted.\n');

      debug(err.stack);
      repl._historyPrev = _replHistoryMessage;
      return ready(null, repl);
    }
  }

  var timer = null;
  var writing = false;
  var pending = false;
  repl.pause();
  // History files are conventionally not readable by others:
  // https://github.com/nodejs/node/issues/3392
  // https://github.com/nodejs/node/pull/3394
  fs.open(historyPath, 'a+', 0o0600, oninit);

  function oninit(err, hnd) {
    if (err) {
      // Cannot open history file.
      // Don't crash, just don't persist history.
      _writeToOutput(repl, '\nError: Could not open history file.\n' +
                           'REPL session history will not be persisted.\n');
      debug(err.stack);

      repl._historyPrev = _replHistoryMessage;
      repl.resume();
      return ready(null, repl);
    }
    fs.close(hnd, onclose);
  }

  function onclose(err) {
    if (err) {
      return ready(err);
    }
    fs.readFile(historyPath, 'utf8', onread);
  }

  function onread(err, data) {
    if (err) {
      return ready(err);
    }

    if (data) {
      repl.history = data.split(/[\n\r]+/, repl.historySize);
    } else if (oldHistoryPath === historyPath) {
      // If pre-v3.0, the user had set NODE_REPL_HISTORY_FILE to
      // ~/.node_repl_history, warn the user about it and proceed.
      _writeToOutput(
        repl,
        '\nThe old repl history file has the same name and location as ' +
          `the new one i.e., ${historyPath} and is empty.\nUsing it as is.\n`);

    } else if (oldHistoryPath) {
      let threw = false;
      try {
        // Pre-v3.0, repl history was stored as JSON.
        // Try and convert it to line separated history.
        const oldReplJSONHistory = fs.readFileSync(oldHistoryPath, 'utf8');

        // Only attempt to use the history if there was any.
        if (oldReplJSONHistory) repl.history = JSON.parse(oldReplJSONHistory);

        if (Array.isArray(repl.history)) {
          repl.history = repl.history.slice(0, repl.historySize);
        } else {
          threw = true;
          _writeToOutput(
            repl,
            '\nError: The old history file data has to be an Array.\n' +
              'REPL session history will not be persisted.\n');
        }
      } catch (err) {
        // Cannot open or parse history file.
        // Don't crash, just don't persist history.
        threw = true;
        const type = err instanceof SyntaxError ? 'parse' : 'open';
        _writeToOutput(repl, `\nError: Could not ${type} old history file.\n` +
                            'REPL session history will not be persisted.\n');
      }
      if (!threw) {
        // Grab data from the older pre-v3.0 JSON NODE_REPL_HISTORY_FILE format.
        _writeToOutput(
          repl,
          '\nConverted old JSON repl history to line-separated history.\n' +
            `The new repl history file can be found at ${historyPath}.\n`);
      } else {
        repl.history = [];
      }
    }

    fs.open(historyPath, 'r+', onhandle);
  }

  function onhandle(err, hnd) {
    if (err) {
      return ready(err);
    }
    fs.ftruncate(hnd, 0, (err) => {
      repl._historyHandle = hnd;
      repl.on('line', online);

      // reading the file data out erases it
      repl.once('flushHistory', function() {
        repl.resume();
        ready(null, repl);
      });
      flushHistory();
    });
  }

  // ------ history listeners ------
  function online() {
    repl._flushing = true;

    if (timer) {
      clearTimeout(timer);
    }

    timer = setTimeout(flushHistory, kDebounceHistoryMS);
  }

  function flushHistory() {
    timer = null;
    if (writing) {
      pending = true;
      return;
    }
    writing = true;
    const historyData = repl.history.join(os.EOL);
    fs.write(repl._historyHandle, historyData, 0, 'utf8', onwritten);
  }

  function onwritten(err, data) {
    writing = false;
    if (pending) {
      pending = false;
      online();
    } else {
      repl._flushing = Boolean(timer);
      if (!repl._flushing) {
        repl.emit('flushHistory');
      }
    }
  }
}


function _replHistoryMessage() {
  if (this.history.length === 0) {
    _writeToOutput(
      this,
      '\nPersistent history support disabled. ' +
        'Set the NODE_REPL_HISTORY environment\nvariable to ' +
        'a valid, user-writable path to enable.\n'
    );
  }
  this._historyPrev = Interface.prototype._historyPrev;
  return this._historyPrev();
}
