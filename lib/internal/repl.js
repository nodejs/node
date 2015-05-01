'use strict';

module.exports = {createRepl: createRepl};

const Interface = require('readline').Interface;
const REPL = require('repl');
const path = require('path');

// XXX(chrisdickinson): The 15ms debounce value is somewhat arbitrary.
// The debounce is to guard against code pasted into the REPL.
const kDebounceHistoryMS = 15;

try {
  // hack for require.resolve("./relative") to work properly.
  module.filename = path.resolve('repl');
} catch (e) {
  // path.resolve('repl') fails when the current working directory has been
  // deleted.  Fall back to the directory name of the (absolute) executable
  // path.  It's not really correct but what are the alternatives?
  const dirname = path.dirname(process.execPath);
  module.filename = path.resolve(dirname, 'repl');
}

// hack for repl require to work properly with node_modules folders
module.paths = require('module')._nodeModulePaths(module.filename);

function createRepl(env, cb) {
  const opts = {
    useGlobal: true,
    ignoreUndefined: false
  };

  if (parseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }
  if (parseInt(env.NODE_DISABLE_COLORS)) {
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
  if (opts.terminal && env.NODE_REPL_HISTORY_PATH) {
    return setupHistory(repl, env.NODE_REPL_HISTORY_PATH, cb);
  }
  repl._historyPrev = _replHistoryMessage;
  cb(null, repl);
}

function setupHistory(repl, historyPath, ready) {
  const fs = require('fs');
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
      try {
        repl.history = JSON.parse(data);
        if (!Array.isArray(repl.history)) {
          throw new Error('Expected array, got ' + typeof repl.history);
        }
        repl.history.slice(-repl.historySize);
      } catch (err) {
        return ready(
            new Error(`Could not parse history data in ${historyPath}.`));
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
    repl.resume();
    return ready(null, repl);
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
    const historyData = JSON.stringify(repl.history, null, 2);
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
        'Set the NODE_REPL_HISTORY_PATH environment variable to ' +
        'a valid, user-writable path to enable.\n'
    );
    this._refreshLine();
  }
  this._historyPrev = Interface.prototype._historyPrev;
  return this._historyPrev();
}
