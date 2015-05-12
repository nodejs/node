'use strict';

const Interface = require('readline').Interface;
const REPL = require('repl');
const path = require('path');
const util = require('util');

const debug = util.debuglog('repl');

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

function createRepl(env, attemptPersistentHistory, cb) {
  const opts = {
    ignoreUndefined: false,
    useGlobal: true
  };

  if (parseInt(env.NODE_NO_READLINE)) {
    opts.terminal = false;
  }
  if (parseInt(env.NODE_FORCE_READLINE)) {
    opts.terminal = true;
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
  if (env.NODE_REPL_HISTORY_FILE && attemptPersistentHistory) {
    return setupHistory(repl, env.NODE_REPL_HISTORY_FILE, function(err) {
      if (err) {
        repl.close();
      }
      cb(err);
    });
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
  fs.readFile(historyPath, {encoding: 'utf8', flag: 'a+'}, ondata);

  function ondata(err, data) {
    if (err) {
      return ready(err);
    }

    debug('loaded %s', historyPath);
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

    getTempFile(ontmpfile);
  }

  function ontmpfile(err, tmpinfo) {
    if (err) {
      return ready(err);
    }
    repl._historyTempInfo = tmpinfo;
    repl.on('line', online);
    repl.on('exit', removeTempFile);
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
    const historyData = JSON.stringify(repl.history || [], null, 2);
    writeAndSwapTemp(historyData, repl._historyTempInfo, historyPath, onflushed);
  }

  function onflushed(err, data) {
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

  function removeTempFile() {
    if (repl._flushing)
      return repl.once('flushHistory', function() {
        removeTempFile();
      });
    repl._flushing = true;
    fs.unlink(repl._historyTempInfo.path, function() {
      onflushed();
    });
  }
}


function _replHistoryMessage() {
  if (this.history.length === 0) {
    this._writeToOutput(
        '\nPersistent history support disabled. ' +
        'Set the NODE_REPL_HISTORY_FILE environment variable to ' +
        'a valid, user-writable path to enable.\n'
    );
    this._refreshLine();
  }
  this._historyPrev = Interface.prototype._historyPrev;
  return this._historyPrev();
}


function getTempFile(ready) {
  var tmpPath = os.tmpdir();
  if (!tmpPath) {
    return ready(new Error('no tmpdir available'));
  }
  tmpPath = path.join(tmpPath, process.pid + '-' + 'repl.tmp');
  fs.open(tmpPath, 'wx', 0o600, function(err, fd) {
    if (err) {
      return ready(err);
    }
    return ready(null, {
      fd: fd,
      path: tmpPath
    });
  });
}

// Write data, sync the fd, close it, overwrite the target
// with a rename, and open a new tmpfile.
//
// We use fs.write instead of writeFile because the latter
// does not accept an fd at the time of writing.
function writeAndSwapTemp(data, tmpInfo, target, ready) {
  debug('writing tmp file... %s', tmpInfo.path, data);
  return fs.write(tmpInfo.fd, data, 0, 'utf8', onwritten);

  function onwritten(err) {
    if (err) return ready(err);
    debug('syncing... %s', tmpInfo.path);
    fs.fsync(tmpInfo.fd, onsync);
  }

  function onsync(err) {
    if (err) return ready(err);
    debug('closing... %s', tmpInfo.path);
    fs.close(tmpInfo.fd, onclose);
  }

  function onclose(err) {
    if (err) return ready(err);
    debug('rename... %s -> %s', tmpInfo.path, target);
    fs.rename(tmpInfo.path, target, onrename);
  }

  function onrename(err) {
    if (err) return ready(err);
    debug('re-loading...');
    getTempFile(ontmpfile);
  }

  function ontmpfile(err, info) {
    if (err) return ready(err);
    tmpInfo.fd = info.fd;
    tmpInfo.path = info.path;
    debug('done!');
    return ready(null);
  }
}
