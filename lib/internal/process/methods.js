'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_UNKNOWN_CREDENTIAL
} = require('internal/errors').codes;
const {
  validateMode,
  validateUint32
} = require('internal/validators');

function setupProcessMethods(_chdir, _cpuUsage, _hrtime, _memoryUsage,
                             _rawDebug, _umask, _initgroups, _setegid,
                             _seteuid, _setgid, _setuid, _setgroups) {
  // Non-POSIX platforms like Windows don't have certain methods.
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

exports.setup = setupProcessMethods;
