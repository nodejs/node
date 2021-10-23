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
  process.setresuid = wrapped.setresuid;
}

// ---- keep the attachment of the wrappers above so that it's easier to ----
// ----              compare the setups side-by-side                    -----

const {
  parseFileMode,
  validateArray,
  validateString
} = require('internal/validators');

function wrapPosixCredentialSetters(credentials) {
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
    setuid: _setuid,
    setresuid: _setresuid
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
    validateArray(groups, 'groups');
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

  function setresuid(ruid, euid, suid) {
    const ids = credentials.getresuid();
    if (ruid === -1) ruid = ids[0];
    else {
      validateId(ruid, 'ruid');
      if (typeof ruid === 'number') ruid |= 0;
    }

    if (euid === -1) euid = ids[1];
    else {
      validateId(euid, 'euid');
      if (typeof euid === 'number') euid |= 0;
    }

    if (suid === -1) suid = ids[2];
    else {
      validateId(suid, 'suid');
      if (typeof suid === 'number') suid |= 0;
    }

    // Result is 0 on success, 0b1xxx if credential is unknown.
    const result = _setresuid(ruid, euid, suid);
    if (result >= 0b1000) {
      const failures = [];
      if (result & 0b0001) failures.push(ruid);
      if (result & 0b0010) failures.push(euid);
      if (result & 0b0100) failures.push(suid);
      throw new ERR_UNKNOWN_CREDENTIAL('User', failures);
    }
  }

  function wrapIdSetter(type, method) {
    return function(id) {
      validateId(id, 'id');
      if (typeof id === 'number') id |= 0;
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
    setresuid,
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
