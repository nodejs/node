/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile:find-down
 * @version 1.0.0
 * @fileoverview Find one or more files by searching the
 *   file system downwards.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var fs = require('fs');
var path = require('path');
var toVFile = require('to-vfile');

/*
 * Methods.
 */

var readdir = fs.readdir;
var stat = fs.stat;
var resolve = path.resolve;
var join = path.join;
var has = Object.prototype.hasOwnProperty;

/*
 * Constants.
 */

var NODE_MODULES = 'node_modules'
var EMPTY = '';
var DOT = '.';
var INCLUDE = 1;
var SKIP = 4;
var BREAK = 8;

/**
 * Check a mask.
 *
 * @param {number} value - Config.
 * @param {number} bitmask - Mask.
 * @return {boolean} - Whether `mask` matches `config`.
 */
function mask(value, bitmask) {
  return (value & bitmask) === bitmask;
}

/**
 * Wrap a string given as a test.
 *
 * A normal string checks for equality to both the filename
 * and extension. A string starting with a `.` checks for
 * that equality too, and also to just the extension.
 *
 * @param {string} filePath - File-name and file-extension
 *   joined by a `.`, or file-extension.
 * @return {Function}
 */
function filePathFactory(filePath) {
    var isExtensionLike = filePath.charAt(0) === DOT;
    var extension = isExtensionLike && filePath.slice(1);

    /**
     * Check whether the given `file` matches the bound
     * value.
     *
     * @param {VFile} file - Virtual file.
     * @return {boolean} - Whether or not there is a match.
     */
    return function (file) {
        var filename = file.filename;
        var parts = [filename];

        if (file.extension) {
            parts.push(file.extension);
        }

        if (
            filePath === parts.join(DOT) ||
            (isExtensionLike && extension === file.extension)
        ) {
            return true;
        }

        if (
            filename.charAt(0) === DOT ||
            filename === NODE_MODULES
        ) {
            return SKIP;
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
    var index;
    var length;
    var tests;

    if (typeof test === 'string') {
        return filePathFactory(test);
    }

    if (typeof test === 'function') {
        return test;
    }

    length = test.length;
    index = -1;
    tests = [];

    while (++index < length) {
        tests[index] = augment(test[index]);
    }

    return function (file) {
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
 * Find files in `filePath`.
 *
 * @example
 *   new FindDown({
 *     'extensions': ['md']
 *   }).visit('~/foo/bar', console.log);
 *
 * @param {Object} state - Information.
 * @param {string} filePath - Path to file or directory.
 * @param {boolean?} one - Whether to search for one or
 *   more files.
 * @param {Function} done - Invoked with a list of zero or
 *   more files.
 */
function visit(state, filePath, one, done) {
    var file;

    /*
     * Prevent walking into places multiple times.
     */

    if (has.call(state.checked, filePath)) {
        done([]);
        return;
    }

    state.checked[filePath] = true;

    file = toVFile(filePath);
    file.quiet = true;

    stat(resolve(filePath), function (err, stats) {
        var real = Boolean(stats);
        var results = [];
        var result;

        if (state.broken || !real) {
            done([]);
        } else {
            result = state.test(file);

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
 * @param {string?} directory - Path to parent directory,
 *   if any.
 * @param {boolean?} one - Whether to search for one or
 *   more files.
 * @param {Function} done - Invoked with a list of zero or
 *   more applicable files.
 */
function visitAll(state, paths, directory, one, done) {
    var result = [];
    var length = paths.length;
    var count = -1;

    /** Invoke `done` when complete */
    function next() {
        count++;

        if (count === length) {
            done(result);
        }
    }

    paths.forEach(function (filePath) {
        visit(state, join(directory || EMPTY, filePath), one, function (files) {
            result = result.concat(files);
            next();
        });
    });

    next();
}

/**
 * Find applicable files.
 *
 * @example
 *   find('.md', '~', console.log);
 *
 * @param {*} test - One or more tests.
 * @param {string|Array.<string>} paths - Directories
 *   to search.
 * @param {Function} callback - Invoked with one or more
 *   files.
 * @param {boolean?} one - Whether to search for one or
 *   more files.
 */
function find(test, paths, callback, one) {
    var state = {
        'broken': false,
        'checked': [],
        'test': augment(test)
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
 * Find a file or a directory downwards.
 *
 * @example
 *   one('package.json', console.log);
 *
 * @param {Function} test - Filter function.
 * @param {(Array.<string>|string)?} [paths]
 *   - Directories to search.
 * @param {Function} callback - Invoked with a result.
 */
function one(test, paths, callback) {
    return find(test, paths, callback, true);
}

/**
 * Find files or directories upwards.
 *
 * @example
 *   all('package.json', console.log);
 *
 * @param {Function} test - Filter function.
 * @param {(Array.<string>|string)?} [paths]
 *   - Directories to search.
 * @param {Function} callback - Invoked with results.
 */
function all(test, paths, callback) {
    return find(test, paths, callback);
}

/*
 * Expose.
 */

var findDown = {};

findDown.INCLUDE = INCLUDE;
findDown.SKIP = SKIP;
findDown.BREAK = BREAK;

findDown.all = all;
findDown.one = one;

module.exports = findDown;
