// This file is a modified version of the rimraf module on npm.
'use strict';

const {
  ArrayPrototypeForEach,
  Promise,
  SafeSet,
} = primordials;

const { Buffer } = require('buffer');
const fs = require('fs');
const {
  lstat,
  lstatSync,
  readdir,
  readdirSync,
  rmdir,
  rmdirSync,
  unlink,
  unlinkSync
} = fs;
const { sep } = require('path');
const { setTimeout } = require('timers');
const { sleep } = require('internal/util');
const notEmptyErrorCodes = new SafeSet(['ENOTEMPTY', 'EEXIST', 'EPERM']);
const retryErrorCodes = new SafeSet(
  ['EBUSY', 'EMFILE', 'ENFILE', 'ENOTEMPTY', 'EPERM']);
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
    } else if (stats.isDirectory()) {
      return _rmdir(path, options, err, callback);
    }

    unlink(path, (err) => {
      if (err) {
        if (err.code === 'ENOENT')
          return callback(null);
        if (err.code === 'EISDIR' || err.code === 'EPERM')
          return _rmdir(path, options, err, callback);
      }

      return callback(err);
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

    ArrayPrototypeForEach(files, (child) => {
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
  }

  try {
    // SunOS lets the root user unlink directories.
    if (stats?.isDirectory())
      _rmdirSync(path, options, null);
    else
      _unlinkSync(path, options);
  } catch (err) {
    if (err.code === 'EISDIR' || err.code === 'EPERM')
      return _rmdirSync(path, options, err);

    if (err.code !== 'ENOENT')
      throw err;
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
      } else if (err.code === 'ENOENT') {
        // The file is already gone.
        return;
      } else if (i === tries) {
        throw err;
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
    if (err.code === 'ENOTDIR') {
      throw originalErr || err;
    }

    if (notEmptyErrorCodes.has(err.code)) {
      // Removing failed. Try removing all children and then retrying the
      // original removal. Windows has a habit of not closing handles promptly
      // when files are deleted, resulting in spurious ENOTEMPTY failures. Work
      // around that issue by retrying on Windows.
      const pathBuf = Buffer.from(path);

      ArrayPrototypeForEach(readdirSync(pathBuf, readdirEncoding), (child) => {
        const childPath = Buffer.concat([pathBuf, separator, child]);

        rimrafSync(childPath, options);
      });

      const tries = options.maxRetries + 1;

      for (let i = 1; i <= tries; i++) {
        try {
          return fs.rmdirSync(path);
        } catch (err) {
          // Only sleep if this is not the last try, and the delay is greater
          // than zero, and an error was encountered that warrants a retry.
          if (retryErrorCodes.has(err.code) &&
              i < tries &&
              options.retryDelay > 0) {
            sleep(i * options.retryDelay);
          } else if (err.code === 'ENOENT') {
            // The file is already gone.
            return;
          } else if (i === tries) {
            throw err;
          }
        }
      }
    }

    throw originalErr || err;
  }
}


module.exports = { rimraf, rimrafPromises, rimrafSync };
