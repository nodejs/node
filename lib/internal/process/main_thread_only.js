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
  parseMode,
  validateUint32,
  validateString
} = require('internal/validators');

const { signals } = internalBinding('constants').os;

// The execution of this function itself should not cause any side effects.
function wrapProcessMethods(binding) {
  // Cache the working directory to prevent lots of lookups. If the working
  // directory is changed by `chdir`, it'll be updated.
  let cachedCwd = '';

  function chdir(directory) {
    validateString(directory, 'directory');
    binding.chdir(directory);
    // Mark cache that it requires an update.
    cachedCwd = '';
  }

  function umask(mask) {
    if (mask !== undefined) {
      mask = parseMode(mask, 'mask');
    }
    return binding.umask(mask);
  }

  function cwd() {
    if (cachedCwd === '')
      cachedCwd = binding.cwd();
    return cachedCwd;
  }

  return {
    chdir,
    umask,
    cwd
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

let Signal;
function isSignal(event) {
  return typeof event === 'string' && signals[event] !== undefined;
}

// Worker threads don't receive signals.
function createSignalHandlers() {
  const signalWraps = new Map();

  // Detect presence of a listener for the special signal types
  function startListeningIfSignal(type) {
    if (isSignal(type) && !signalWraps.has(type)) {
      if (Signal === undefined)
        Signal = internalBinding('signal_wrap').Signal;
      const wrap = new Signal();

      wrap.unref();

      wrap.onsignal = process.emit.bind(process, type, type);

      const signum = signals[type];
      const err = wrap.start(signum);
      if (err) {
        wrap.close();
        throw errnoException(err, 'uv_signal_start');
      }

      signalWraps.set(type, wrap);
    }
  }

  function stopListeningIfSignal(type) {
    const wrap = signalWraps.get(type);
    if (wrap !== undefined && process.listenerCount(type) === 0) {
      wrap.close();
      signalWraps.delete(type);
    }
  }

  return {
    startListeningIfSignal,
    stopListeningIfSignal
  };
}

module.exports = {
  wrapProcessMethods,
  createSignalHandlers,
  wrapPosixCredentialSetters
};
