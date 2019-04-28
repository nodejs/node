'use strict';

const { defaults, fixWinEPERM, fixWinEPERMSync, rmdir, rmdirSync } = require('internal/shutil');
const assert = require('assert');

const isWindows = process.platform === 'win32';

// for EMFILE handling
let timeout = 0;

function rmtree(p, options, cb) {
  if (typeof options === 'function') {
    cb = options;
    options = {};
  }

  assert(p, 'rmtree: missing path');
  assert.equal(typeof p, 'string', 'rmtree: path should be a string');
  assert.equal(typeof cb, 'function', 'rmtree: callback function required');
  assert(options, 'rmtree: invalid options argument provided');
  assert.equal(typeof options, 'object', 'rmtree: options should be object');

  defaults(options);

  let busyTries = 0;

  rmtree_(p, options, function CB(er) {
    if (er) {
      if (
        (er.code === 'EBUSY' || er.code === 'ENOTEMPTY' || er.code === 'EPERM') &&
        busyTries < options.maxBusyTries
      ) {
        busyTries++;
        let time = busyTries * 100;
        // try again, with the same exact callback as this one.
        return setTimeout(function() {
          rmtree_(p, options, CB);
        }, time);
      }

      // this one won't happen if graceful-fs is used.
      if (er.code === 'EMFILE' && timeout < options.emfileWait) {
        return setTimeout(function() {
          rmtree_(p, options, CB);
        }, timeout++);
      }

      // already gone
      if (er.code === 'ENOENT') er = null;

      cb(er);
    }

    timeout = 0;
    cb();
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
function rmtree_(p, options, cb) {
  assert(p);
  assert(options);
  assert(typeof cb === 'function');

  // sunos lets the root user unlink directories, which is... weird.
  // so we have to lstat here and make sure it's not a dir.
  options.lstat(p, function(er, st) {
    if (er && er.code === 'ENOENT') return cb(null);

    // Windows can EPERM on stat.  Life is suffering.
    if (er && er.code === 'EPERM' && isWindows) fixWinEPERM(p, options, er, cb);

    if (st && st.isDirectory()) return rmdir(p, options, er, cb);

    options.unlink(p, function(er) {
      if (er) {
        if (er.code === 'ENOENT') return cb(null);
        if (er.code === 'EPERM')
          return isWindows ? fixWinEPERM(p, options, er, cb) : rmdir(p, options, er, cb);
        if (er.code === 'EISDIR') return rmdir(p, options, er, cb);
      }
      return cb(er);
    });
  });
}

// this looks simpler, and is strictly *faster*, but will
// tie up the JavaScript thread and fail on excessively
// deep directory trees.
function rmtreeSync(p, options) {
  options = options || {};
  defaults(options);

  assert(p, 'rmtree: missing path');
  assert.equal(typeof p, 'string', 'rmtree: path should be a string');
  assert(options, 'rmtree: missing options');
  assert.equal(typeof options, 'object', 'rmtree: options should be object');

  try {
    var st = options.lstatSync(p);
  } catch (er) {
    if (er.code === 'ENOENT') return;

    // Windows can EPERM on stat.  Life is suffering.
    if (er.code === 'EPERM' && isWindows) fixWinEPERMSync(p, options, er);
  }

  try {
    // sunos lets the root user unlink directories, which is... weird.
    if (st && st.isDirectory()) rmdirSync(p, options, null);
    else options.unlinkSync(p);
  } catch (er) {
    if (er.code === 'ENOENT') return;
    if (er.code === 'EPERM')
      return isWindows ? fixWinEPERMSync(p, options, er) : rmdirSync(p, options, er);
    if (er.code !== 'EISDIR') throw er;

    rmdirSync(p, options, er);
  }
}

module.exports = {
  rmtree,
  rmtreeSync
};
