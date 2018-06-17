'use strict';

// This file contains process bootstrappers that can only be
// run in the main thread

const {
  errnoException,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_UNKNOWN_CREDENTIAL
  }
} = require('internal/errors');
const {
  validateMode,
  validateUint32
} = require('internal/validators');

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

  process.chdir = function chdir(directory) {
    if (typeof directory !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('directory', 'string', directory);
    }
    return _chdir(directory);
  };

  process.umask = function umask(mask) {
    if (mask === undefined) {
      // Get the mask
      return _umask(mask);
    }
    mask = validateMode(mask, 'mask');
    return _umask(mask);
  };
}

function setupPosixMethods(_initgroups, _setegid, _seteuid,
                           _setgid, _setuid, _setgroups) {

  process.initgroups = function initgroups(user, extraGroup) {
    validateId(user, 'user');
    validateId(extraGroup, 'extraGroup');
    // Result is 0 on success, 1 if user is unknown, 2 if group is unknown.
    const result = _initgroups(user, extraGroup);
    if (result === 1) {
      throw new ERR_UNKNOWN_CREDENTIAL('User', user);
    } else if (result === 2) {
      throw new ERR_UNKNOWN_CREDENTIAL('Group', extraGroup);
    }
  };

  process.setegid = function setegid(id) {
    return execId(id, 'Group', _setegid);
  };

  process.seteuid = function seteuid(id) {
    return execId(id, 'User', _seteuid);
  };

  process.setgid = function setgid(id) {
    return execId(id, 'Group', _setgid);
  };

  process.setuid = function setuid(id) {
    return execId(id, 'User', _setuid);
  };

  process.setgroups = function setgroups(groups) {
    if (!Array.isArray(groups)) {
      throw new ERR_INVALID_ARG_TYPE('groups', 'Array', groups);
    }
    for (var i = 0; i < groups.length; i++) {
      validateId(groups[i], `groups[${i}]`);
    }
    // Result is 0 on success. A positive integer indicates that the
    // corresponding group was not found.
    const result = _setgroups(groups);
    if (result > 0) {
      throw new ERR_UNKNOWN_CREDENTIAL('Group', groups[result - 1]);
    }
  };

  function execId(id, type, method) {
    validateId(id, 'id');
    // Result is 0 on success, 1 if credential is unknown.
    const result = method(id);
    if (result === 1) {
      throw new ERR_UNKNOWN_CREDENTIAL(type, id);
    }
  }

  function validateId(id, name) {
    if (typeof id === 'number') {
      validateUint32(id, name);
    } else if (typeof id !== 'string') {
      throw new ERR_INVALID_ARG_TYPE(name, ['number', 'string'], id);
    }
  }
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
