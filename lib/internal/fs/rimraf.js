// This file is a modified version of the rimraf module on npm. It has been
// modified in the following ways:
// - Use of the assert module has been replaced with core's error system.
// - All code related to the glob dependency has been removed.
// - Bring your own custom fs module is not currently supported.
// - Some basic code cleanup.
'use strict';
const {
  chmod,
  chmodSync,
  lstat,
  lstatSync,
  readdir,
  readdirSync,
  rmdir,
  rmdirSync,
  stat,
  statSync,
  unlink,
  unlinkSync
} = require('fs');
const { join } = require('path');
const { setTimeout } = require('timers');
const notEmptyErrorCodes = new Set(['ENOTEMPTY', 'EEXIST', 'EPERM']);
const isWindows = process.platform === 'win32';
const epermHandler = isWindows ? fixWinEPERM : _rmdir;
const epermHandlerSync = isWindows ? fixWinEPERMSync : _rmdirSync;
const numRetries = isWindows ? 100 : 1;


function rimraf(path, options, callback) {
  let timeout = 0;  // For EMFILE handling.
  let busyTries = 0;

  _rimraf(path, options, function CB(err) {
    if (err) {
      if ((err.code === 'EBUSY' || err.code === 'ENOTEMPTY' ||
           err.code === 'EPERM') && busyTries < options.maxBusyTries) {
        busyTries++;
        return setTimeout(_rimraf, busyTries * 100, path, options, CB);
      }

      if (err.code === 'EMFILE' && timeout < options.emfileWait)
        return setTimeout(_rimraf, timeout++, path, options, CB);

      // The file is already gone.
      if (err.code === 'ENOENT')
        err = null;
    }

    callback(err);
  });
}


function _rimraf(path, options, callback) {
  // SunOS lets the root user unlink directories. Use lstat here to make sure
  // it's not a directory.
  lstat(path, (err, stats) => {
    if (err) {
      if (err.code === 'ENOENT')
        return callback(null);

      // Windows can EPERM on stat.
      if (isWindows && err.code === 'EPERM')
        return fixWinEPERM(path, options, err, callback);
    } else if (stats.isDirectory()) {
      return _rmdir(path, options, err, callback);
    }

    unlink(path, (err) => {
      if (err) {
        if (err.code === 'ENOENT')
          return callback(null);
        if (err.code === 'EISDIR')
          return _rmdir(path, options, err, callback);
        if (err.code === 'EPERM') {
          return epermHandler(path, options, err, callback);
        }
      }

      return callback(err);
    });
  });
}


function fixWinEPERM(path, options, originalErr, callback) {
  chmod(path, 0o666, (err) => {
    if (err)
      return callback(err.code === 'ENOENT' ? null : originalErr);

    stat(path, (err, stats) => {
      if (err)
        return callback(err.code === 'ENOENT' ? null : originalErr);

      if (stats.isDirectory())
        _rmdir(path, options, originalErr, callback);
      else
        unlink(path, callback);
    });
  });
}


function _rmdir(path, options, originalErr, callback) {
  rmdir(path, (err) => {
    if (err) {
      if (notEmptyErrorCodes.has(err.code))
        return _rmchildren(path, options, callback);
      if (err.code === 'ENOTDIR')
        return callback(originalErr);
    }

    callback(err);
  });
}


function _rmchildren(path, options, callback) {
  readdir(path, (err, files) => {
    if (err)
      return callback(err);

    let numFiles = files.length;

    if (numFiles === 0)
      return rmdir(path, callback);

    let done = false;

    files.forEach((child) => {
      rimraf(join(path, child), options, (err) => {
        if (done)
          return;

        if (err) {
          done = true;
          return callback(err);
        }

        numFiles--;
        if (numFiles === 0)
          rmdir(path, callback);
      });
    });
  });
}


function rimrafPromises(path, options) {
  return new Promise((resolve, reject) => {
    rimraf(path, options, (err) => {
      if (err)
        return reject(err);

      resolve();
    });
  });
}


function rimrafSync(path, options) {
  let stats;

  try {
    stats = lstatSync(path);
  } catch (err) {
    if (err.code === 'ENOENT')
      return;

    // Windows can EPERM on stat.
    if (isWindows && err.code === 'EPERM')
      fixWinEPERMSync(path, options, err);
  }

  try {
    // SunOS lets the root user unlink directories.
    if (stats !== undefined && stats.isDirectory())
      _rmdirSync(path, options, null);
    else
      unlinkSync(path);
  } catch (err) {
    if (err.code === 'ENOENT')
      return;
    if (err.code === 'EPERM')
      return epermHandlerSync(path, options, err);
    if (err.code !== 'EISDIR')
      throw err;

    _rmdirSync(path, options, err);
  }
}


function _rmdirSync(path, options, originalErr) {
  try {
    rmdirSync(path);
  } catch (err) {
    if (err.code === 'ENOENT')
      return;
    if (err.code === 'ENOTDIR')
      throw originalErr;

    if (notEmptyErrorCodes.has(err.code)) {
      // Removing failed. Try removing all children and then retrying the
      // original removal. Windows has a habit of not closing handles promptly
      // when files are deleted, resulting in spurious ENOTEMPTY failures. Work
      // around that issue by retrying on Windows.
      readdirSync(path).forEach((child) => {
        rimrafSync(join(path, child), options);
      });

      for (let i = 0; i < numRetries; i++) {
        try {
          return rmdirSync(path, options);
        } catch {} // Ignore errors.
      }
    }
  }
}


function fixWinEPERMSync(path, options, originalErr) {
  try {
    chmodSync(path, 0o666);
  } catch (err) {
    if (err.code === 'ENOENT')
      return;

    throw originalErr;
  }

  let stats;

  try {
    stats = statSync(path);
  } catch (err) {
    if (err.code === 'ENOENT')
      return;

    throw originalErr;
  }

  if (stats.isDirectory())
    _rmdirSync(path, options, originalErr);
  else
    unlinkSync(path);
}


module.exports = { rimraf, rimrafPromises, rimrafSync };
