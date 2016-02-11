/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module vfile:find-up
 * @version 0.0.0
 * @fileoverview Find one or more files by searching the
 *   file system upwards.
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
var resolve = path.resolve;
var dirname = path.dirname;
var basename = path.basename;

/*
 * Constants.
 */

var DOT = '.';
var INCLUDE = 1;
var BREAK = 4;

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
        var name = file.filename + (file.extension ? DOT + file.extension : '');

        return filePath === name ||
            (isExtensionLike && extension === file.extension);
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
        index = -1;

        while (++index < length) {
            if (tests[index](file)) {
                return true;
            }
        }

        return false;
    }
}

/**
 * Find files and directories upwards.
 *
 * @example
 *   find('package.json', '.', console.log);
 *
 * @private
 * @param {Function} test - Filter function.
 * @param {string?} directory - Path to directory to search.
 * @param {Function} callback - Invoked with results.
 * @param {boolean?} [one] - When `true`, returns the
 *   first result (`string`), otherwise, returns an array
 *   of strings.
 */
function find(test, directory, callback, one) {
    var results = [];
    var currentDirectory;

    test = augment(test);

    if (!callback) {
        callback = directory;
        directory = null;
    }

    currentDirectory = directory ? resolve(directory) : process.cwd();

    /**
     * Test a file-path and check what should be done with
     * the resulting file.
     * Returns true when iteration should stop.
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

    /** TODO */
    function once(childDirectory) {
        if (handle(currentDirectory) === true) {
            return;
        }

        readdir(currentDirectory, function (err, entries) {
            var length = entries ? entries.length : 0;
            var index = -1;
            var entry;

            if (err) {
                entries = [];
            }

            while (++index < length) {
                entry = entries[index];

                if (entry !== childDirectory) {
                    if (handle(resolve(currentDirectory, entry)) === true) {
                        return;
                    }
                }
            }

            childDirectory = currentDirectory;
            currentDirectory = dirname(currentDirectory);

            if (currentDirectory === childDirectory) {
                callback(null, one ? null : results);
                return;
            }

            once(basename(childDirectory));
        });
    }

    once();
}

/**
 * Find a file or a directory upwards.
 *
 * @example
 *   findOne('package.json', console.log);
 *
 * @param {Function} test - Filter function.
 * @param {string?} [directory] - Path to directory to search.
 * @param {Function} callback - Invoked with a result.
 */
function findOne(test, directory, callback) {
    return find(test, directory, callback, true);
}

/**
 * Find files or directories upwards.
 *
 * @example
 *   findAll('package.json', console.log);
 *
 * @param {Function} test - Filter function.
 * @param {string?} [directory] - Path to directory to search.
 * @param {Function} callback - Invoked with results.
 */
function findAll(test, directory, callback) {
    return find(test, directory, callback);
}

/*
 * Expose.
 */

var findUp = {};

findUp.INCLUDE = INCLUDE;
findUp.BREAK = BREAK;

findUp.one = findOne;
findUp.all = findAll;

module.exports = findUp;
