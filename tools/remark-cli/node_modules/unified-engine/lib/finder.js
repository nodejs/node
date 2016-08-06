/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:finder
 * @fileoverview Get applicable input files from
 *   the file system to be processed, respecting
 *   ignored paths and applicable extensions.
 */

'use strict';

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:finder');
var globby = require('globby');
var magic = require('glob').hasMagic;
var minimatch = require('minimatch');
var toVFile = require('to-vfile');
var findDown = require('vfile-find-down');

/* Expose. */
module.exports = Finder;

/* Methods. */
var resolve = path.resolve;
var relative = path.relative;
var stat = fs.statSync;

/**
 * Construct a new finder.
 *
 * @constructor
 * @class {Finder}
 * @param {Object} [options={}] - Settings.
 */
function Finder(options) {
  this.settings = options;
}

/* Methods. */
Finder.prototype.find = find;

/**
 * Find files matching `patterns` and the bound settings.
 *
 * @this {Finder}
 * @param {Array.<string>} patterns - Globs and file-paths
 *   to search for.
 * @param {Function} callback - Callback to invoke when
 *   done.
 */
function find(patterns, callback) {
  var self = this;
  var skip = self.settings.silentlyIgnore;
  var ignore = self.settings.ignore;
  var extensions = self.settings.extensions;
  var cwd = self.settings.cwd;
  var negations = ignore.negations();
  var globs = [];
  var given = [];
  var failed = [];
  var oneFileMode;

  if (!extensions || !extensions.length) {
    debug('Cannot search for files without `extensions`');
  }

  patterns.forEach(function (pattern) {
    var file;
    var stats;
    var message;

    if (magic(pattern)) {
      globs.push(pattern);
      oneFileMode = false;
    } else {
      given.push(resolve(cwd, pattern));

      try {
        stats = stat(resolve(cwd, pattern));

        oneFileMode = stats.isFile();
      } catch (err) {
        file = toVFile(pattern);
        oneFileMode = false;

        message = file.message('No such file or directory');
        message.fatal = true;

        failed.push(file);
      }
    }
  });

  self.oneFileMode = given.length > 1 ? false : oneFileMode;

  /**
   * Test to check if `file` should be included.
   *
   * @param {VFile} file - Virtual file or directory.
   * @return {*} - Results for `vfile-find-down`.
   */
  function test(file, stats) {
    var filePath = file.path;
    var extension = (file.extname || '').slice(1);
    var isGiven = given.indexOf(filePath) !== -1;
    var isFile = stats.isFile();
    var message;

    /* If the file or directory should be ignored,
     * is hidden and not negated, skip it.
     *
     * If it is both ignored and given, trigger a
     * warning if not `silentlyIgnore`d. */
    if (ignore.check(filePath)) {
      if (isGiven) {
        if (skip) {
          return findDown.SKIP;
        }

        message = file.message('Cannot process specified file: it’s ignored');
        message.fatal = true;

        return findDown.SKIP | findDown.INCLUDE;
      }

      /* If there are negation patterns, directories
       * are still searched for files as their paths
       * can be made visible by negation patterns. */
      return negations ? false : findDown.SKIP;
    }

    /* Do not include non-files. */
    if (!isFile) {
      return false;
    }

    /* If the file is given, matches a glob, or has
     * a known extension, include it. */
    return isGiven ||
      matches(relative(cwd, filePath), globs) ||
      (extension && extensions.indexOf(extension) !== -1);
  }

  globby(patterns, {cwd: cwd}).then(function (filePaths) {
    filePaths = filePaths.map(function (filePath) {
      return resolve(cwd, filePath);
    });

    findDown.all(test, filePaths, function (err, files) {
      /* Fix `filePath` in relation to `cwd`. */
      files.forEach(function (file) {
        file.cwd = cwd;
        file.dirname = relative(cwd, file.dirname);
        file.history = [file.path];
      });

      files = failed.concat(files);

      /* Sort alphabetically. */
      files.sort(function (left, right) {
        return left.path > right.path;
      });

      /* Mark as given.  This allows outputting files,
       * which can be pretty dangerous, so it’s “hidden”. */
      files.forEach(function (file) {
        file.data.unifiedEngineGiven = true;
      });

      callback(err, files);
    });
  }, callback);
}

/**
 * Check if `filePath` matches some `patterns`.
 *
 * @param {string} filePath - File location.
 * @param {string} patterns - Globs pattern.
 * @return {boolean} - Whether `file` matches `pattern`.
 */
function matches(filePath, patterns) {
  return patterns.some(function (pattern) {
    return minimatch(filePath, pattern) || minimatch(filePath, pattern + '/**');
  });
}
