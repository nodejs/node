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
  validateUint32,
  validateString
} = require('internal/validators');

// The execution of this function itself should not cause any side effects.
function wrapProcessMethods(binding) {
  function chdir(directory) {
    validateString(directory, 'directory');
    return binding.chdir(directory);
  }

  function umask(mask) {
    if (mask === undefined) {
      // Get the mask
      return binding.umask(mask);
    }
    mask = validateMode(mask, 'mask');
    return binding.umask(mask);
  }

  return {
    chdir,
    umask
  };
}

function wrapPosixCredentialSetters(credentials) {
  const {
    initgroups: _initgroups,
    setgroups: _setgroups,
    setegid: _setegid,
    seteuid: _seteuid,
    setgid: _setgid,
    setuid: _setuid
  } = credentials;

  function initgroups(user, extraGroup) {
    validateId(user, 'user');
    validateId(extraGroup, 'extraGroup');
    // Result is 0 on success, 1 if user is unknown, 2 if group is unknown.
    const result = _initgroups(user, extraGroup);
    if (result === 1) {
      throw new ERR_UNKNOWN_CREDENTIAL('User', user);
    } else if (result === 2) {
      throw new ERR_UNKNOWN_CREDENTIAL('Group', extraGroup);
    }
  }

  function setgroups(groups) {
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
  }

  function wrapIdSetter(type, method) {
    return function(id) {
      validateId(id, 'id');
      // Result is 0 on success, 1 if credential is unknown.
      const result = method(id);
      if (result === 1) {
        throw new ERR_UNKNOWN_CREDENTIAL(type, id);
      }
    };
  }

  function validateId(id, name) {
    if (typeof id === 'number') {
      validateUint32(id, name);
    } else if (typeof id !== 'string') {
      throw new ERR_INVALID_ARG_TYPE(name, ['number', 'string'], id);
    }
  }

  return {
    initgroups,
    setgroups,
    setegid: wrapIdSetter('Group', _setegid),
    seteuid: wrapIdSetter('User', _seteuid),
    setgid: wrapIdSetter('Group', _setgid),
    setuid: wrapIdSetter('User', _setuid)
  };
}

// Worker threads don't receive signals.
function setupSignalHandlers(internalBinding) {
  const constants = internalBinding('constants').os.signals;
  const signalWraps = Object.create(null);
  let Signal;

  function isSignal(event) {
    return typeof event === 'string' && constants[event] !== undefined;
  }

  // Detect presence of a listener for the special signal types
  process.on('newListener', function(type) {
    if (isSignal(type) && signalWraps[type] === undefined) {
      if (Signal === undefined)
        Signal = internalBinding('signal_wrap').Signal;
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
  const assert = require('assert').strict;

  const fd = parseInt(process.env.NODE_CHANNEL_FD, 10);
  assert(fd >= 0);

  // Make sure it's not accidentally inherited by child processes.
  delete process.env.NODE_CHANNEL_FD;

  require('child_process')._forkChild(fd);
  assert(process.send);
}

module.exports = {
  wrapProcessMethods,
  setupSignalHandlers,
  setupChildProcessIpcChannel,
  wrapPosixCredentialSetters
};
