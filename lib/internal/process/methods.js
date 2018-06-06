'use strict';

function setupProcessMethods() {
  // Non-POSIX platforms like Windows don't have certain methods.
  if (process.setgid !== undefined) {
    setupPosixMethods();
  }

  const { chdir: _chdir, umask: _umask } = process;

  process.chdir = chdir;
  process.umask = umask;

  function chdir(...args) {
    return _chdir(...args);
  }

  function umask(...args) {
    return _umask(...args);
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

  function initgroups(...args) {
    return _initgroups(...args);
  }

  function setegid(...args) {
    return _setegid(...args);
  }

  function seteuid(...args) {
    return _seteuid(...args);
  }

  function setgid(...args) {
    return _setgid(...args);
  }

  function setuid(...args) {
    return _setuid(...args);
  }

  function setgroups(...args) {
    return _setgroups(...args);
  }
}

exports.setup = setupProcessMethods;
