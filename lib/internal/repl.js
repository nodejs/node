'use strict';

const I18N = require('internal/messages');
const debug = require('util').debuglog('repl');
const Interface = require('readline').Interface;
const REPL = require('repl');
const path = require('path');
const fs = require('fs');
const os = require('os');

module.exports = Object.create(REPL);
module.exports.createInternalRepl = createRepl;

// XXX(chrisdickinson): The 15ms debounce value is somewhat arbitrary.
// The debounce is to guard against code pasted into the REPL.
const kDebounceHistoryMS = 15;

function createRepl(env, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts;
    opts = null;
  }
  opts = opts || {
    ignoreUndefined: false,
    terminal: process.stdout.isTTY,
    useGlobal: true
  };

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
    'sloppy': REPL.REPL_MODE_SLOPPY,
    'magic': REPL.REPL_MODE_MAGIC
  }[String(env.NODE_REPL_MODE).toLowerCase().trim()];

  if (opts.replMode === undefined) {
    opts.replMode = REPL.REPL_MODE_MAGIC;
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
  if (opts.terminal && env.NODE_REPL_HISTORY !== '') {
    return setupHistory(repl, env.NODE_REPL_HISTORY,
                        env.NODE_REPL_HISTORY_FILE, cb);
  }
  repl._historyPrev = _replHistoryMessage;
  cb(null, repl);
}

function setupHistory(repl, historyPath, oldHistoryPath, ready) {
  if (!historyPath) {
    try {
      historyPath = path.join(os.homedir(), '.node_repl_history');
    } catch (err) {
      repl._writeToOutput(`\n${I18N(I18N.REPL_HOME_DIRECTORY)}\n`);
      repl._refreshLine();

      debug(err.stack);
      repl._historyPrev = _replHistoryMessage;
      return ready(null, repl);
    }
  }

  var timer = null;
  var writing = false;
  var pending = false;
  repl.pause();
  fs.open(historyPath, 'a+', oninit);

  function oninit(err, hnd) {
    if (err) {
      // Cannot open history file.
      // Don't crash, just don't persist history.
      repl._writeToOutput(`\n${I18N(I18N.REPL_CANNOT_OPEN_HISTORY)}\n`);
      repl._refreshLine();
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
      repl._writeToOutput(
        `\n${I18N(I18N.REPL_HISTORY_SAME_NAME, historyPath)}\n`);
      repl._refreshLine();

    }  else if (oldHistoryPath) {
      // Grab data from the older pre-v3.0 JSON NODE_REPL_HISTORY_FILE format.
      repl._writeToOutput(
        `\n${I18N(I18N.REPL_CONVERTING_HISTORY, historyPath)}\n`);
      repl._refreshLine();

      try {
        // Pre-v3.0, repl history was stored as JSON.
        // Try and convert it to line separated history.
        const oldReplJSONHistory = fs.readFileSync(oldHistoryPath, 'utf8');

        // Only attempt to use the history if there was any.
        if (oldReplJSONHistory) repl.history = JSON.parse(oldReplJSONHistory);

        if (!Array.isArray(repl.history)) {
          throw new I18N.Error(I18N.INVALID_ARG_TYPE, 'repl.history', 'array');
        }
        repl.history = repl.history.slice(0, repl.historySize);
      } catch (err) {
        if (err.code !== 'ENOENT') {
          return ready(new I18N.Error(I18N.REPL_CANNOT_PARSE_HISTORY,
                                     oldHistoryPath));
        }
      }
    }

    fs.open(historyPath, 'w', onhandle);
  }

  function onhandle(err, hnd) {
    if (err) {
      return ready(err);
    }
    repl._historyHandle = hnd;
    repl.on('line', online);

    // reading the file data out erases it
    repl.once('flushHistory', function() {
      repl.resume();
      ready(null, repl);
    });
    flushHistory();
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
    this._writeToOutput(`\n${I18N(I18N.REPL_HISTORY_DISABLED)}\n`);
    this._refreshLine();
  }
  this._historyPrev = Interface.prototype._historyPrev;
  return this._historyPrev();
}
