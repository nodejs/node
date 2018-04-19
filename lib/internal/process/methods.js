'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_UNKNOWN_CREDENTIAL
} = require('internal/errors').codes;
const {
  validateUint32
} = require('internal/validators');

function setupProcessMethods() {
  // Non-POSIX platforms like Windows don't have certain methods.
  if (process.setgid !== undefined) {
    setupPosixMethods();
  }

  const {
    chdir: _chdir,
    umask: _umask,
  } = process;

  process.chdir = chdir;
  process.umask = umask;

  function chdir(directory) {
    if (typeof directory !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('directory', 'string', directory);
    }
    return _chdir(directory);
  }

  const octalReg = /^[0-7]+$/;
  function umask(mask) {
    if (typeof mask === 'undefined') {
      return _umask(mask);
    }

    if (typeof mask === 'number') {
      validateUint32(mask, 'mask');
      return _umask(mask);
    }

    if (typeof mask === 'string') {
      if (!octalReg.test(mask)) {
        throw new ERR_INVALID_ARG_VALUE('mask', mask,
                                        'must be an octal string');
      }
      const octal = Number.parseInt(mask, 8);
      validateUint32(octal, 'mask');
      return _umask(octal);
    }

    throw new ERR_INVALID_ARG_TYPE('mask', ['number', 'string', 'undefined'],
                                   mask);
  }
}

function setupPosixMethods() {
  const {
    initgroups: _initgroups,
    setegid: _setegid,
    seteuid: _seteuid,
    setgid: _setgid,
    setuid: _setuid,
    setgroups: _setgroups
  } = process;

  process.initgroups = initgroups;
  process.setegid = setegid;
  process.seteuid = seteuid;
  process.setgid = setgid;
  process.setuid = setuid;
  process.setgroups = setgroups;

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

  function setegid(id) {
    return execId(id, 'Group', _setegid);
  }

  function seteuid(id) {
    return execId(id, 'User', _seteuid);
  }

  function setgid(id) {
    return execId(id, 'Group', _setgid);
  }

  function setuid(id) {
    return execId(id, 'User', _setuid);
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

exports.setup = setupProcessMethods;
