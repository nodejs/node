'use strict';

var path = require('path');
var fs = require('fs');
var glob = require('glob');
var vfile = require('to-vfile');
var xtend = require('xtend');
var hidden = require('is-hidden');
var string = require('x-is-string');

var readdir = fs.readdir;
var stat = fs.stat;
var join = path.join;
var relative = path.relative;
var resolve = path.resolve;
var basename = path.basename;
var extname = path.extname;
var magic = glob.hasMagic;

module.exports = find;

/* Search `patterns`, a mix of globs, paths, and files. */
function find(input, options, callback) {
  expand(input, options, done);

  function done(err, result) {
    /* istanbul ignore if - glob errors are unusual.
     * other errors are on the vfile results. */
    if (err) {
      callback(err);
    } else {
      callback(null, {oneFileMode: oneFileMode(result), files: result.output});
    }
  }
}

/* Expand the given glob patterns, search given and found
 * directories, and map to vfiles. */
function expand(input, options, next) {
  var cwd = options.cwd;
  var paths = [];
  var actual = 0;
  var expected = 0;
  var failed;

  input.forEach(each);

  if (!expected) {
    search(paths, options, done);
  }

  function each(file) {
    if (string(file)) {
      if (magic(file)) {
        expected++;
        glob(file, {cwd: cwd}, one);
      } else {
        /* `relative` to make the paths canonical. */
        file = relative(cwd, resolve(cwd, file)) || '.';
        paths.push(file);
      }
    } else {
      file.cwd = cwd;
      file.path = relative(cwd, file.path);
      file.history = [file.path];
      paths.push(file);
    }
  }

  function one(err, files) {
    /* istanbul ignore if - glob errors are unusual. */
    if (failed) {
      return;
    }

    /* istanbul ignore if - glob errors are unusual. */
    if (err) {
      failed = true;
      done(err);
    } else {
      actual++;
      paths = paths.concat(files);

      if (actual === expected) {
        search(paths, options, done);
      }
    }
  }

  function done(err, files) {
    /* istanbul ignore if - `search` currently does not give errors. */
    if (err) {
      next(err);
    } else {
      next(null, {input: paths, output: files});
    }
  }
}

/* Search `paths`. */
function search(input, options, next) {
  var cwd = options.cwd;
  var silent = options.silentlyIgnore;
  var nested = options.nested;
  var extensions = options.extensions;
  var files = [];
  var expected = 0;
  var actual = 0;

  input.forEach(each);

  if (!expected) {
    next(null, files);
  }

  return each;

  function each(file) {
    var part = base(file);

    if (nested && (hidden(part) || part === 'node_modules')) {
      return;
    }

    expected++;

    statAndIgnore(file, options, handle);

    function handle(err, result) {
      var ignored = result && result.ignored;
      var dir = result && result.stats && result.stats.isDirectory();

      if (ignored && (nested || silent)) {
        return one(null, []);
      }

      if (!ignored && dir) {
        return readdir(resolve(cwd, filePath(file)), directory);
      }

      if (nested && !dir && extensions.length !== 0 && extensions.indexOf(extname(file)) === -1) {
        return one(null, []);
      }

      file = vfile(file);
      file.cwd = cwd;

      if (ignored) {
        try {
          file.fail('Cannot process specified file: it’s ignored');
        } catch (err) {}
      }

      if (err && err.code === 'ENOENT') {
        try {
          file.fail(err.syscall === 'stat' ? 'No such file or directory' : err);
        } catch (err) {}
      }

      one(null, [file]);
    }

    function directory(err, basenames) {
      var file;

      /* istanbul ignore if - Should not happen often: the directory
       * is `stat`ed first, which was ok, but reading it is not. */
      if (err) {
        file = vfile(filePath(file));
        file.cwd = cwd;

        try {
          file.fail('Cannot read directory');
        } catch (err) {}

        one(null, [file]);
      } else {
        search(basenames.map(concat), xtend(options, {nested: true}), one);
      }
    }

    /* Error is never given. Always given `results`. */
    function one(_, results) {
      /* istanbul ignore else - always given. */
      if (results) {
        files = files.concat(results);
      }

      actual++;

      if (actual === expected) {
        next(null, files);
      }
    }

    function concat(value) {
      return join(filePath(file), value);
    }
  }
}

function statAndIgnore(file, options, callback) {
  var ignore = options.ignore;
  var fp = resolve(options.cwd, filePath(file));
  var expected = 1;
  var actual = 0;
  var stats;
  var ignored;

  if (!file.contents) {
    expected++;
    stat(fp, handleStat);
  }

  ignore.check(fp, handleIgnore);

  function handleStat(err, value) {
    stats = value;
    one(err);
  }

  function handleIgnore(err, value) {
    ignored = value;
    one(err);
  }

  function one(err) {
    actual++;

    if (err) {
      callback(err);
      actual = -1;
    } else if (actual === expected) {
      callback(null, {stats: stats, ignored: ignored});
    }
  }
}

function base(file) {
  return string(file) ? basename(file) : file.basename;
}

function filePath(file) {
  return string(file) ? file : file.path;
}

function oneFileMode(result) {
  return result.output.length === 1 &&
    result.input.length === 1 &&
    result.output[0].path === result.input[0];
}
