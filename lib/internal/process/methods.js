'use strict';

const {
  isMainThread
} = require('internal/worker');

function setupProcessMethods(_chdir, _cpuUsage, _hrtime, _memoryUsage,
                             _rawDebug, _umask, _initgroups, _setegid,
                             _seteuid, _setgid, _setuid, _setgroups) {
  // Non-POSIX platforms like Windows don't have certain methods.
  // Workers also lack these methods since they change process-global state.
  if (!isMainThread)
    return;

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

exports.setup = setupProcessMethods;
