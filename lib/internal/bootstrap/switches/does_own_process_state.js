'use strict';

const credentials = internalBinding('credentials');
const rawMethods = internalBinding('process_methods');

process.abort = rawMethods.abort;
process.umask = wrappedUmask;
process.chdir = wrappedChdir;
process.cwd = wrappedCwd;

if (credentials.implementsPosixCredentials) {
  const wrapped = wrapPosixCredentialSetters(credentials);

  process.initgroups = wrapped.initgroups;
  process.setgroups = wrapped.setgroups;
  process.setegid = wrapped.setegid;
  process.seteuid = wrapped.seteuid;
  process.setgid = wrapped.setgid;
  process.setuid = wrapped.setuid;
}

// ---- keep the attachment of the wrappers above so that it's easier to ----
// ----              compare the setups side-by-side                    -----

const {
  parseFileMode,
  validateString
} = require('internal/validators');

function wrapPosixCredentialSetters(credentials) {
  const {
    ArrayIsArray,
  } = primordials;
  const {
    codes: {
      ERR_INVALID_ARG_TYPE,
      ERR_UNKNOWN_CREDENTIAL
    }
  } = require('internal/errors');
  const {
    validateUint32
  } = require('internal/validators');

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
    if (!ArrayIsArray(groups)) {
      throw new ERR_INVALID_ARG_TYPE('groups', 'Array', groups);
    }
    for (let i = 0; i < groups.length; i++) {
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

// Cache the working directory to prevent lots of lookups. If the working
// directory is changed by `chdir`, it'll be updated.
let cachedCwd = '';

function wrappedChdir(directory) {
  validateString(directory, 'directory');
  rawMethods.chdir(directory);
  // Mark cache that it requires an update.
  cachedCwd = '';
}

function wrappedUmask(mask) {
  if (mask !== undefined) {
    mask = parseFileMode(mask, 'mask');
  }
  return rawMethods.umask(mask);
}

function wrappedCwd() {
  if (cachedCwd === '')
    cachedCwd = rawMethods.cwd();
  return cachedCwd;
}
