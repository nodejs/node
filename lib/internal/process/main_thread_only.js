'use strict';

// This file contains process bootstrappers that can only be
// run in the main thread

const {
  errnoException
} = require('internal/errors');

const {
  setupProcessStdio,
  getMainThreadStdio
} = require('internal/process/stdio');

const assert = require('assert').strict;

function setupStdio() {
  setupProcessStdio(getMainThreadStdio());
}

// Non-POSIX platforms like Windows don't have certain methods.
// Workers also lack these methods since they change process-global state.
function setupProcessMethods(_chdir, _umask, _initgroups, _setegid,
                             _seteuid, _setgid, _setuid, _setgroups) {
  if (_setgid !== undefined) {
    setupPosixMethods(_initgroups, _setegid, _seteuid,
                      _setgid, _setuid, _setgroups);
  }

  process.chdir = function chdir(...args) {
    return _chdir(...args);
  };

  process.umask = function umask(...args) {
    return _umask(...args);
  };
}

function setupPosixMethods(_initgroups, _setegid, _seteuid,
                           _setgid, _setuid, _setgroups) {

  process.initgroups = function initgroups(...args) {
    return _initgroups(...args);
  };

  process.setegid = function setegid(...args) {
    return _setegid(...args);
  };

  process.seteuid = function seteuid(...args) {
    return _seteuid(...args);
  };

  process.setgid = function setgid(...args) {
    return _setgid(...args);
  };

  process.setuid = function setuid(...args) {
    return _setuid(...args);
  };

  process.setgroups = function setgroups(...args) {
    return _setgroups(...args);
  };
}

// Worker threads don't receive signals.
function setupSignalHandlers() {
  const constants = process.binding('constants').os.signals;
  const signalWraps = Object.create(null);
  let Signal;

  function isSignal(event) {
    return typeof event === 'string' && constants[event] !== undefined;
  }

  // Detect presence of a listener for the special signal types
  process.on('newListener', function(type) {
    if (isSignal(type) && signalWraps[type] === undefined) {
      if (Signal === undefined)
        Signal = process.binding('signal_wrap').Signal;
      const wrap = new Signal();

      wrap.unref();

      wrap.onsignal = process.emit.bind(process, type, type);

      const signum = constants[type];
      const err = wrap.start(signum);
      if (err) {
        wrap.close();
        throw errnoException(err, 'uv_signal_start');
      }

      signalWraps[type] = wrap;
    }
  });

  process.on('removeListener', function(type) {
    if (signalWraps[type] !== undefined && this.listenerCount(type) === 0) {
      signalWraps[type].close();
      delete signalWraps[type];
    }
  });

  // re-arm pre-existing signal event registrations
  // with this signal wrap capabilities.
  process.eventNames().forEach((ev) => {
    if (isSignal(ev))
      process.emit('newListener', ev);
  });
}

function setupChildProcessIpcChannel() {
  // If we were spawned with env NODE_CHANNEL_FD then load that up and
  // start parsing data from that stream.
  if (process.env.NODE_CHANNEL_FD) {
    const fd = parseInt(process.env.NODE_CHANNEL_FD, 10);
    assert(fd >= 0);

    // Make sure it's not accidentally inherited by child processes.
    delete process.env.NODE_CHANNEL_FD;

    require('child_process')._forkChild(fd);
    assert(process.send);
  }
}

module.exports = {
  setupStdio,
  setupProcessMethods,
  setupSignalHandlers,
  setupChildProcessIpcChannel
};
