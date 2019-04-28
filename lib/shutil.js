'use strict';

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const isWindows = process.platform === 'win32';
const _0666 = parseInt('666', 8);

// for EMFILE handling
let timeout = 0;

function defaults(options) {
  const methods = ['unlink', 'chmod', 'stat', 'lstat', 'rmdir', 'readdir'];

  methods.forEach(function(method) {
    options[method] = options[method] || fs[method];
    method = method + 'Sync';
    options[method] = options[method] || fs[method];
  });

  options.maxBusyTries = options.maxBusyTries || 3;
  options.emfileWait = options.emfileWait || 1000;
}

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

function fixWinEPERM(p, options, er, cb) {
  assert(p);
  assert(options);
  assert(typeof cb === 'function');
  if (er) assert(er instanceof Error);

  options.chmod(p, _0666, function(er2) {
    if (er2) cb(er2.code === 'ENOENT' ? null : er);
    else
      options.stat(p, function(er3, stats) {
        if (er3) cb(er3.code === 'ENOENT' ? null : er);
        else if (stats.isDirectory()) rmdir(p, options, er, cb);
        else options.unlink(p, cb);
      });
  });
}

function fixWinEPERMSync(p, options, er) {
  assert(p);
  assert(options);
  if (er) assert(er instanceof Error);

  try {
    options.chmodSync(p, _0666);
  } catch (er2) {
    if (er2.code === 'ENOENT') return;
    else throw er;
  }

  try {
    var stats = options.statSync(p);
  } catch (er3) {
    if (er3.code === 'ENOENT') return;
    else throw er;
  }

  if (stats.isDirectory()) rmdirSync(p, options, er);
  else options.unlinkSync(p);
}

function rmdir(p, options, originalEr, cb) {
  assert(p);
  assert(options);
  if (originalEr) assert(originalEr instanceof Error);
  assert(typeof cb === 'function');

  // try to rmdir first, and only readdir on ENOTEMPTY or EEXIST (SunOS)
  // if we guessed wrong, and it's not a directory, then
  // raise the original error.
  options.rmdir(p, function(er) {
    if (er && (er.code === 'ENOTEMPTY' || er.code === 'EEXIST' || er.code === 'EPERM'))
      rmkids(p, options, cb);
    else if (er && er.code === 'ENOTDIR') cb(originalEr);
    else cb(er);
  });
}

function rmkids(p, options, cb) {
  assert(p);
  assert(options);
  assert(typeof cb === 'function');

  options.readdir(p, function(er, files) {
    if (er) return cb(er);
    var n = files.length;
    if (n === 0) return options.rmdir(p, cb);
    var errState;
    files.forEach(function(f) {
      rmtree(path.join(p, f), options, function(er) {
        if (errState) return;
        if (er) return cb((errState = er));
        if (--n === 0) options.rmdir(p, cb);
      });
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

function rmdirSync(p, options, originalEr) {
  assert(p);
  assert(options);
  if (originalEr) assert(originalEr instanceof Error);

  try {
    options.rmdirSync(p);
  } catch (er) {
    if (er.code === 'ENOENT') return;
    if (er.code === 'ENOTDIR') throw originalEr;
    if (er.code === 'ENOTEMPTY' || er.code === 'EEXIST' || er.code === 'EPERM')
      rmkidsSync(p, options);
  }
}

function rmkidsSync(p, options) {
  assert(p);
  assert(options);
  options.readdirSync(p).forEach(function(f) {
    rmtreeSync(path.join(p, f), options);
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
      var ret = options.rmdirSync(p, options);
      threw = false;
      return ret;
    } finally {
      if (++i < retries && threw) continue;
    }
  } while (true);
}

module.exports = {
  rmtree,
  rmtreeSync
};
