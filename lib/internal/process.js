'use strict';

var _lazyConstants = null;

function lazyConstants() {
  if (!_lazyConstants) {
    _lazyConstants = process.binding('constants');
  }
  return _lazyConstants;
}

exports.setup_hrtime = setup_hrtime;
exports.setupConfig = setupConfig;
exports.setupKillAndExit = setupKillAndExit;
exports.setupSignalHandlers = setupSignalHandlers;
exports.setupChannel = setupChannel;
exports.setupRawDebug = setupRawDebug;


const assert = process.assert = function(x, msg) {
  if (!x) throw new Error(msg || 'assertion error');
};


function setup_hrtime() {
  const _hrtime = process.hrtime;
  const hrValues = new Uint32Array(3);

  process.hrtime = function hrtime(ar) {
    _hrtime(hrValues);

    if (typeof ar !== 'undefined') {
      if (Array.isArray(ar)) {
        const sec = (hrValues[0] * 0x100000000 + hrValues[1]) - ar[0];
        const nsec = hrValues[2] - ar[1];
        return [nsec < 0 ? sec - 1 : sec, nsec < 0 ? nsec + 1e9 : nsec];
      }

      throw new TypeError('process.hrtime() only accepts an Array tuple');
    }

    return [
      hrValues[0] * 0x100000000 + hrValues[1],
      hrValues[2]
    ];
  };
}


function setupConfig(_source) {
  // NativeModule._source
  // used for `process.config`, but not a real module
  var config = _source.config;
  delete _source.config;

  // strip the gyp comment line at the beginning
  config = config.split('\n')
      .slice(1)
      .join('\n')
      .replace(/"/g, '\\"')
      .replace(/'/g, '"');

  process.config = JSON.parse(config, function(key, value) {
    if (value === 'true') return true;
    if (value === 'false') return false;
    return value;
  });
}


function setupKillAndExit() {

  const kExitTimer = Symbol('kExitTimer');

  function exitWithTimeout(code, timeout) {
    // The way this works is simple. When exitWithTimeout is called, the
    // number of listeners registered for the `exitingSoon` event on
    // process is grabbed. A callback is created and passed to each of
    // the listeners on emit. When each is done doing it's thing, it
    // invokes the callback, which counts down. Once the counter hits
    // zero, the real process.exit is called. In the meantime, an unref'd
    // timer is created to run proecss.exit() at a given timeout just in
    // case the cleanup code takes too long. Returns true if the method
    // schedules the exit, returns false if an exit has already been
    // scheduled.
    if (process[kExitTimer] !== undefined)
      return false;   // Already called!
    var count = process.listenerCount('exitingSoon');
    function ready() {
      count--;
      if (count <= 0) {
        // If we're here, we beat the exit timeout, clear it and exit.
        clearTimeout(process[kExitTimer]);
        process.exit(code);
      }
    }
    process[kExitTimer] = setTimeout(() => {
      process.exit(code);
    }, timeout);
    process[kExitTimer].unref();
    process.emit('exitingSoon', ready);
    return true;
  }

  process.exit = function(code, timeout) {
    if (code || code === 0)
      process.exitCode = code;

    if (timeout) {
      timeout |= 0;
      if (timeout > 0)
        return exitWithTimeout(code, timeout);
    }

    if (!process._exiting) {
      process._exiting = true;
      process.emit('exit', process.exitCode || 0);
    }
    process.reallyExit(process.exitCode || 0);
  };

  process.kill = function(pid, sig) {
    var err;

    if (pid != (pid | 0)) {
      throw new TypeError('invalid pid');
    }

    // preserve null signal
    if (0 === sig) {
      err = process._kill(pid, 0);
    } else {
      sig = sig || 'SIGTERM';
      if (lazyConstants()[sig] &&
          sig.slice(0, 3) === 'SIG') {
        err = process._kill(pid, lazyConstants()[sig]);
      } else {
        throw new Error(`Unknown signal: ${sig}`);
      }
    }

    if (err) {
      var errnoException = require('util')._errnoException;
      throw errnoException(err, 'kill');
    }

    return true;
  };
}


function setupSignalHandlers() {
  // Load events module in order to access prototype elements on process like
  // process.addListener.
  var signalWraps = {};

  function isSignal(event) {
    return typeof event === 'string' &&
           event.slice(0, 3) === 'SIG' &&
           lazyConstants().hasOwnProperty(event);
  }

  // Detect presence of a listener for the special signal types
  process.on('newListener', function(type, listener) {
    if (isSignal(type) &&
        !signalWraps.hasOwnProperty(type)) {
      var Signal = process.binding('signal_wrap').Signal;
      var wrap = new Signal();

      wrap.unref();

      wrap.onsignal = function() { process.emit(type); };

      var signum = lazyConstants()[type];
      var err = wrap.start(signum);
      if (err) {
        wrap.close();
        var errnoException = require('util')._errnoException;
        throw errnoException(err, 'uv_signal_start');
      }

      signalWraps[type] = wrap;
    }
  });

  process.on('removeListener', function(type, listener) {
    if (signalWraps.hasOwnProperty(type) && this.listenerCount(type) === 0) {
      signalWraps[type].close();
      delete signalWraps[type];
    }
  });
}


function setupChannel() {
  // If we were spawned with env NODE_CHANNEL_FD then load that up and
  // start parsing data from that stream.
  if (process.env.NODE_CHANNEL_FD) {
    var fd = parseInt(process.env.NODE_CHANNEL_FD, 10);
    assert(fd >= 0);

    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_CHANNEL_FD;

    var cp = require('child_process');

    // Load tcp_wrap to avoid situation where we might immediately receive
    // a message.
    // FIXME is this really necessary?
    process.binding('tcp_wrap');

    cp._forkChild(fd);
    assert(process.send);
  }
}


function setupRawDebug() {
  var format = require('util').format;
  var rawDebug = process._rawDebug;
  process._rawDebug = function() {
    rawDebug(format.apply(null, arguments));
  };
}
