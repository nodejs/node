'use strict';

const fs = require('fs');
const { join } = require('path');
const { validatePath } = require('internal/fs/utils');
const { setTimeout } = require('timers');
const {
  codes: { ERR_INVALID_ARG_TYPE, ERR_INVALID_CALLBACK }
} = require('internal/errors');
const isWindows = process.platform === 'win32';
const _0666 = parseInt('666', 8);

// For EMFILE handling
let timeout = 0;

function rmtree(path, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }

  validatePath(path);
  validateCallback(callback);

  options = getOptions(options);

  let busyTries = 0;

  rmtree_(path, options, function CB(er) {
    if (er) {
      if (
        (er.code === 'EBUSY' ||
          er.code === 'ENOTEMPTY' ||
          er.code === 'EPERM') &&
        busyTries < options.maxBusyTries
      ) {
        busyTries++;
        const time = busyTries * 100;
        // Try again, with the same exact callback as this one.
        return setTimeout(function() {
          rmtree_(path, options, CB);
        }, time);
      }

      // This one won't happen if graceful-fs is used.
      if (er.code === 'EMFILE' && timeout < options.emfileWait) {
        return setTimeout(function() {
          rmtree_(path, options, CB);
        }, timeout++);
      }

      // Already gone
      if (er.code === 'ENOENT') er = null;

      callback(er);
    }

    timeout = 0;
    callback();
  });
}

// Two possible strategies.
// 1. Assume it's a file.  unlink it, then do the dir stuff on EPERM or EISDIR
// 2. Assume it's a directory.  readdir, then do the file stuff on ENOTDIR
//
// Both result in an extra syscall when you guess wrong.  However, there
// are likely far more normal files in the world than directories.  This
// is based on the assumption that a the average number of files per
// directory is >= 1.
//
// If anyone ever complains about this, then I guess the strategy could
// be made configurable somehow.  But until then, YAGNI.
function rmtree_(path, options, callback) {
  validatePath(path);
  validateOptions(options);
  validateCallback(callback);

  // Sunos lets the root user unlink directories, which is... weird.
  // So we have to lstat here and make sure it's not a dir.
  options.lstat(path, function(er, st) {
    if (er && er.code === 'ENOENT') return callback(null);

    // Windows can EPERM on stat. Life is suffering.
    if (er && er.code === 'EPERM' && isWindows) {
      fixWinEPERM(path, options, er, callback);
    }

    if (st && st.isDirectory()) return rmdir(path, options, er, callback);

    options.unlink(path, function(er) {
      if (er) {
        if (er.code === 'ENOENT') return callback(null);
        if (er.code === 'EPERM')
          return isWindows ?
            fixWinEPERM(path, options, er, callback) :
            rmdir(path, options, er, callback);
        if (er.code === 'EISDIR') return rmdir(path, options, er, callback);
      }
      return callback(er);
    });
  });
}

// This looks simpler, and is strictly *faster*, but will
// tie up the JavaScript thread and fail on excessively
// deep directory trees.
function rmtreeSync(path, options) {
  options = options || {};
  options = getOptions(options);

  validatePath(path);

  let st;

  try {
    st = options.lstatSync(path);
  } catch (er) {
    if (er.code === 'ENOENT') return;

    // Windows can EPERM on stat.  Life is suffering.
    if (er.code === 'EPERM' && isWindows) fixWinEPERMSync(path, options, er);
  }

  try {
    // Sunos lets the root user unlink directories, which is... weird.
    if (st && st.isDirectory()) rmdirSync(path, options, null);
    else options.unlinkSync(path);
  } catch (er) {
    if (er.code === 'ENOENT') return;
    if (er.code === 'EPERM')
      return isWindows ?
        fixWinEPERMSync(path, options, er) :
        rmdirSync(path, options, er);
    if (er.code !== 'EISDIR') throw er;

    rmdirSync(path, options, er);
  }
}

function validateOptions(options) {
  if (typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }
}

function validateCallback(callback) {
  if (typeof callback === 'function') {
    return callback;
  }

  throw new ERR_INVALID_CALLBACK(callback);
}

function validateError(error) {
  if (!(error instanceof Error)) {
    throw new ERR_INVALID_ARG_TYPE('error', 'Error', error);
  }
}

function getOptions(options) {
  const methods = ['unlink', 'chmod', 'stat', 'lstat', 'rmdir', 'readdir'];

  validateOptions(options);

  methods.forEach(function(method) {
    options[method] = options[method] || fs[method];
    method = method + 'Sync';
    options[method] = options[method] || fs[method];
  });

  options.maxBusyTries = options.maxBusyTries || 3;
  options.emfileWait = options.emfileWait || 1000;

  return options;
}

function fixWinEPERM(path, options, error, callback) {
  validatePath(path);
  validateOptions(options);
  if (error) validateError(error);
  validateCallback(callback);

  options.chmod(path, _0666, function(er2) {
    if (er2) callback(er2.code === 'ENOENT' ? null : error);
    else
      options.stat(path, function(er3, stats) {
        if (er3) callback(er3.code === 'ENOENT' ? null : error);
        else if (stats.isDirectory()) rmdir(path, options, error, callback);
        else options.unlink(path, callback);
      });
  });
}

function fixWinEPERMSync(path, options, error) {
  validatePath(path);
  validateOptions(options);
  if (error) validateError(error);

  let stats;

  try {
    options.chmodSync(path, _0666);
  } catch (er2) {
    if (er2.code === 'ENOENT') return;
    else throw error;
  }

  try {
    stats = options.statSync(path);
  } catch (er3) {
    if (er3.code === 'ENOENT') return;
    else throw error;
  }

  if (stats.isDirectory()) rmdirSync(path, options, error);
  else options.unlinkSync(path);
}

function rmdir(path, options, originalEr, callback) {
  validatePath(path);
  validateOptions(options);
  if (originalEr) validateError(originalEr);
  validateCallback(callback);

  // Try to rmdir first, and only readdir on ENOTEMPTY or EEXIST (SunOS)
  // if we guessed wrong, and it's not a directory, then
  // raise the original error.
  options.rmdir(path, function(er) {
    if (er &&
        (er.code === 'ENOTEMPTY' ||
          er.code === 'EEXIST' ||
          er.code === 'EPERM')
    )
      rmkids(path, options, callback);
    else if (er && er.code === 'ENOTDIR') callback(originalEr);
    else callback(er);
  });
}

function rmdirSync(path, options, originalEr) {
  validatePath(path);
  validateOptions(options);
  if (originalEr) validateError(originalEr);

  try {
    options.rmdirSync(path);
  } catch (er) {
    if (er.code === 'ENOENT') return;
    if (er.code === 'ENOTDIR') throw originalEr;
    if (er.code === 'ENOTEMPTY' || er.code === 'EEXIST' || er.code === 'EPERM')
      rmkidsSync(path, options);
  }
}

function rmkids(path, options, callback) {
  validatePath(path);
  validateOptions(options);
  validateCallback(callback);

  options.readdir(path, function(er, files) {
    if (er) return callback(er);
    var n = files.length;
    if (n === 0) return options.rmdir(path, callback);
    var errState;
    files.forEach(function(f) {
      rmtree(join(path, f), options, function(er) {
        if (errState) return;
        if (er) return callback((errState = er));
        if (--n === 0) options.rmdir(path, callback);
      });
    });
  });
}

function rmkidsSync(path, options) {
  validatePath(path);
  validateOptions(options);

  options.readdirSync(path).forEach(function(f) {
    rmtreeSync(join(path, f), options);
  });

  // We only end up here once we got ENOTEMPTY at least once, and
  // at this point, we are guaranteed to have removed all the kids.
  // So, we know that it won't be ENOENT or ENOTDIR or anything else.
  // try really hard to delete stuff on windows, because it has a
  // PROFOUNDLY annoying habit of not closing handles promptly when
  // files are deleted, resulting in spurious ENOTEMPTY errors.
  var retries = isWindows ? 100 : 1;
  var i = 0;
  do {
    var threw = true;
    try {
      var ret = options.rmdirSync(path, options);
      threw = false;
      return ret;
    } finally {
      // This is taken directly from rimraf. Fixing the lint error could
      // subtly change the behavior
      // eslint-disable-next-line
      if (++i < retries && threw) continue;
    }
  } while (true);
}

module.exports = {
  rmtree,
  rmtreeSync
};
