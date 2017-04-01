/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile:find-up
 * @fileoverview Find files by searching the file system upwards.
 */

'use strict';

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var toVFile = require('to-vfile');

/* Constants. */
var INCLUDE = 1;
var BREAK = 4;

/* Expose. */
exports.INCLUDE = INCLUDE;
exports.BREAK = BREAK;
exports.one = findOne;
exports.all = findAll;

/* Methods. */
var readdir = fs.readdir;
var resolve = path.resolve;
var dirname = path.dirname;
var basename = path.basename;

/**
 * Find a file or a directory upwards.
 *
 * @param {Function} test - Filter function.
 * @param {string?} [cwd] - Path to search from.
 * @param {Function} callback - Invoked with a result.
 */
function findOne(test, cwd, callback) {
  return find(test, cwd, callback, true);
}

/**
 * Find files or directories upwards.
 *
 * @param {Function} test - Filter function.
 * @param {string?} [cwd] - Directory to search from.
 * @param {Function} callback - Invoked with results.
 */
function findAll(test, cwd, callback) {
  return find(test, cwd, callback);
}

/**
 * Find applicable files.
 *
 * @param {*} test - Filter.
 * @param {string?} cwd - Path to search from.
 * @param {Function} callback - Invoked with results.
 * @param {boolean?} [one] - When `true`, returns the
 *   first result (`string`), otherwise, returns an array
 *   of strings.
 */
function find(test, cwd, callback, one) {
  var results = [];
  var current;

  test = augment(test);

  if (!callback) {
    callback = cwd;
    cwd = null;
  }

  current = cwd ? resolve(cwd) : process.cwd();

  once();

  return;

  /**
   * Test a file and check what should be done with
   * the resulting file.
   *
   * @param {string} filePath - Path to file.
   * @return {boolean} - `true` when `callback` is
   *   invoked and iteration should stop.
   */
  function handle(filePath) {
    var file = toVFile(filePath);
    var result = test(file);

    if (mask(result, INCLUDE)) {
      if (one) {
        callback(null, file);
        return true;
      }

      results.push(file);
    }

    if (mask(result, BREAK)) {
      callback(null, one ? null : results);
      return true;
    }
  }

  /** Check one directory. */
  function once(child) {
    if (handle(current) === true) {
      return;
    }

    readdir(current, function (err, entries) {
      var length = entries ? entries.length : 0;
      var index = -1;
      var entry;

      if (err) {
        entries = [];
      }

      while (++index < length) {
        entry = entries[index];

        if (entry !== child && handle(resolve(current, entry)) === true) {
          return;
        }
      }

      child = current;
      current = dirname(current);

      if (current === child) {
        callback(null, one ? null : results);
        return;
      }

      once(basename(child));
    });
  }
}

/** Augment `test` */
function augment(test) {
  if (typeof test === 'function') {
    return test;
  }

  return typeof test === 'string' ? testString(test) : multiple(test);
}

/** Check multiple tests. */
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

  function check(file) {
    return test === file.basename || test === file.extname;
  }
}

/** Check a mask. */
function mask(value, bitmask) {
  return (value & bitmask) === bitmask;
}
