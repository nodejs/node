const path = require('path');
const fs = require('fs');

const _0666 = parseInt('666', 8);

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
  defaults,
  fixWinEPERM,
  fixWinEPERMSync,
  rmdir,
  rmdirSync
}
