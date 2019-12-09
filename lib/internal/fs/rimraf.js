// This file is a modified version of the rimraf module on npm. It has been
// modified in the following ways:
// - Use of the assert module has been replaced with core's error system.
// - All code related to the glob dependency has been removed.
// - Bring your own custom fs module is not currently supported.
// - Some basic code cleanup.
'use strict';

const {
  Promise,
  Set,
} = primordials;

const { Buffer } = require('buffer');
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
const { sep } = require('path');
const { setTimeout } = require('timers');
const { sleep } = require('internal/util');
const notEmptyErrorCodes = new Set(['ENOTEMPTY', 'EEXIST', 'EPERM']);
const retryErrorCodes = new Set(
  ['EBUSY', 'EMFILE', 'ENFILE', 'ENOTEMPTY', 'EPERM']);
const isWindows = process.platform === 'win32';
const epermHandler = isWindows ? fixWinEPERM : _rmdir;
const epermHandlerSync = isWindows ? fixWinEPERMSync : _rmdirSync;
const readdirEncoding = 'buffer';
const separator = Buffer.from(sep);


function rimraf(path, options, callback) {
  let retries = 0;

  _rimraf(path, options, function CB(err) {
    if (err) {
      if (retryErrorCodes.has(err.code) && retries < options.maxRetries) {
        retries++;
        const delay = retries * options.retryDelay;
        return setTimeout(_rimraf, delay, path, options, CB);
      }

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
  const pathBuf = Buffer.from(path);

  readdir(pathBuf, readdirEncoding, (err, files) => {
    if (err)
      return callback(err);

    let numFiles = files.length;

    if (numFiles === 0)
      return rmdir(path, callback);

    let done = false;

    files.forEach((child) => {
      const childPath = Buffer.concat([pathBuf, separator, child]);

      rimraf(childPath, options, (err) => {
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
      _unlinkSync(path, options);
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


function _unlinkSync(path, options) {
  const tries = options.maxRetries + 1;

  for (let i = 1; i <= tries; i++) {
    try {
      return unlinkSync(path);
    } catch (err) {
      // Only sleep if this is not the last try, and the delay is greater
      // than zero, and an error was encountered that warrants a retry.
      if (retryErrorCodes.has(err.code) &&
          i < tries &&
          options.retryDelay > 0) {
        sleep(i * options.retryDelay);
      }
    }
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
      const pathBuf = Buffer.from(path);

      readdirSync(pathBuf, readdirEncoding).forEach((child) => {
        const childPath = Buffer.concat([pathBuf, separator, child]);

        rimrafSync(childPath, options);
      });

      const tries = options.maxRetries + 1;

      for (let i = 1; i <= tries; i++) {
        try {
          return rmdirSync(path, options);
        } catch (err) {
          // Only sleep if this is not the last try, and the delay is greater
          // than zero, and an error was encountered that warrants a retry.
          if (retryErrorCodes.has(err.code) &&
              i < tries &&
              options.retryDelay > 0) {
            sleep(i * options.retryDelay);
          }
        }
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
    _unlinkSync(path, options);
}


module.exports = { rimraf, rimrafPromises, rimrafSync };
