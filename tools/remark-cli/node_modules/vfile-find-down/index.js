/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile:find-down
 * @fileoverview Find files by searching the file system downwards.
 */

'use strict';

/* eslint-disable handle-callback-err, max-params */

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var has = require('has');
var vfile = require('to-vfile');

/* Constants. */
var NODE_MODULES = 'node_modules';
var INCLUDE = 1;
var SKIP = 4;
var BREAK = 8;

/* Expose. */
exports.INCLUDE = INCLUDE;
exports.SKIP = SKIP;
exports.BREAK = BREAK;
exports.all = all;
exports.one = one;

/* Methods. */
var readdir = fs.readdir;
var stat = fs.stat;
var resolve = path.resolve;
var join = path.join;

/**
 * Find a file or a directory downwards.
 *
 * @param {Function} test - Filter function.
 * @param {Array|string?} [paths] - Directories to search.
 * @param {Function} callback - Invoked with a result.
 */
function one(test, paths, callback) {
  return find(test, paths, callback, true);
}

/**
 * Find files or directories upwards.
 *
 * @param {Function} test - Filter function.
 * @param {Array|string?} [paths] - Directories to search.
 * @param {Function} callback - Invoked with results.
 */
function all(test, paths, callback) {
  return find(test, paths, callback);
}

/**
 * Find applicable files.
 *
 * @param {*} test - One or more tests.
 * @param {string|Array} paths - Directories to search.
 * @param {Function} callback
 * @param {boolean?} one - Whether to search for one or
 *   more files.
 */
function find(test, paths, callback, one) {
  var state = {
    broken: false,
    checked: [],
    test: augment(test)
  };

  if (!callback) {
    callback = paths;
    paths = [process.cwd()];
  } else if (typeof paths === 'string') {
    paths = [paths];
  }

  return visitAll(state, paths, null, one, function (result) {
    callback(null, one ? result[0] || null : result);
  });
}

/**
 * Find files in `filePath`.
 *
 * @param {Object} state - Information.
 * @param {string} filePath - Path to file or directory.
 * @param {boolean?} one - Search for one file.
 * @param {Function} done - Invoked with a list of zero or
 *   more files.
 */
function visit(state, filePath, one, done) {
  var file;

  /* Don’t walk into places multiple times. */
  if (has(state.checked, filePath)) {
    done([]);
    return;
  }

  state.checked[filePath] = true;

  file = vfile(filePath);

  stat(resolve(filePath), function (err, stats) {
    var real = Boolean(stats);
    var results = [];
    var result;

    if (state.broken || !real) {
      done([]);
    } else {
      result = state.test(file, stats);

      if (mask(result, INCLUDE)) {
        results.push(file);

        if (one) {
          state.broken = true;
          return done(results);
        }
      }

      if (mask(result, BREAK)) {
        state.broken = true;
      }

      if (state.broken || !stats.isDirectory() || mask(result, SKIP)) {
        return done(results);
      }

      readdir(filePath, function (err, entries) {
        visitAll(state, entries, filePath, one, function (files) {
          done(results.concat(files));
        });
      });
    }
  });
}

/**
 * Find files in `paths`.  Returns a list of
 * applicable files.
 *
 * @example
 *   visitAll('.md', ['bar'], '~/foo', false, console.log);
 *
 * @param {Object} state - Information.
 * @param {Array.<string>} paths - Path to files and
 *   directories.
 * @param {string?} cwd - Path to parent, if any.
 * @param {boolean?} one - Whether to search for one or
 *   more files.
 * @param {Function} done - Invoked with a list of zero or
 *   more applicable files.
 */
function visitAll(state, paths, cwd, one, done) {
  var length = paths.length;
  var count = -1;
  var result = [];

  paths.forEach(function (filePath) {
    visit(state, join(cwd || '', filePath), one, function (files) {
      result = result.concat(files);
      next();
    });
  });

  next();

  /** Invoke `done` when complete */
  function next() {
    count++;

    if (count === length) {
      done(result);
    }
  }
}

/**
 * Augment `test` from several supported values to a
 * function returning a boolean.
 *
 * @param {string|Function|Array.<string|Function>} test
 *   - Augmented test.
 * @return {Function} - test
 */
function augment(test) {
  if (typeof test === 'function') {
    return test;
  }

  return typeof test === 'string' ? testString(test) : multiple(test);
}

/**
 * Wrap a string given as a test.
 *
 * A normal string checks for equality to both the filename
 * and extension. A string starting with a `.` checks for
 * that equality too, and also to just the extension.
 *
 * @param {string} test - Basename or extname.
 * @return {Function} - File-path test.
 */
function testString(test) {
  return check;

  /**
   * Check whether the given `file` matches the bound
   * value.
   *
   * @param {VFile} file - Virtual file.
   * @return {boolean} - Whether or not there is a match.
   */
  function check(file) {
    var basename = file.basename;

    if (test === basename || test === file.extname) {
      return true;
    }

    if (basename.charAt(0) === '.' || basename === NODE_MODULES) {
      return SKIP;
    }
  }
}

function multiple(test) {
  var length = test.length;
  var index = -1;
  var tests = [];

  while (++index < length) {
    tests[index] = augment(test[index]);
  }

  return check;

  function check(file) {
    var result;

    index = -1;

    while (++index < length) {
      result = tests[index](file);

      if (result) {
        return result;
      }
    }

    return false;
  }
}

function mask(value, bitmask) {
  return (value & bitmask) === bitmask;
}
