'use strict';

const Interface = require('readline').Interface;
const REPL = require('repl');
const path = require('path');
const fs = require('fs');
const os = require('os');
const debug = require('util').debuglog('repl');

module.exports = Object.create(REPL);
module.exports.createInternalRepl = createRepl;

// XXX(chrisdickinson): The 15ms debounce value is somewhat arbitrary.
// The debounce is to guard against code pasted into the REPL.
const kDebounceHistoryMS = 15;

// XXX(chrisdickinson): hack to make sure that the internal debugger
// uses the original repl.
function replStart() {
  return REPL.start.apply(REPL, arguments);
}

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
      repl._writeToOutput('\nError: Could not get the home directory.\n' +
                          'REPL session history will not be persisted.\n');
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
      return ready(err);
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
    } else if (oldHistoryPath) {
      // Grab data from the older pre-v3.0 JSON NODE_REPL_HISTORY_FILE format.
      repl._writeToOutput(
          '\nConverting old JSON repl history to line-separated history.\n' +
          `The new repl history file can be found at ${historyPath}.\n`);
      repl._refreshLine();

      try {
        repl.history = JSON.parse(fs.readFileSync(oldHistoryPath, 'utf8'));
        if (!Array.isArray(repl.history)) {
          throw new Error('Expected array, got ' + typeof repl.history);
        }
        repl.history = repl.history.slice(0, repl.historySize);
      } catch (err) {
        if (err.code !== 'ENOENT') {
          return ready(
            new Error(`Could not parse history data in ${oldHistoryPath}.`));
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
    this._writeToOutput(
        '\nPersistent history support disabled. ' +
        'Set the NODE_REPL_HISTORY environment\nvariable to ' +
        'a valid, user-writable path to enable.\n'
    );
    this._refreshLine();
  }
  this._historyPrev = Interface.prototype._historyPrev;
  return this._historyPrev();
}
