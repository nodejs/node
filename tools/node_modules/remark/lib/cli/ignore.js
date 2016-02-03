/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:ignore
 * @version 3.2.2
 * @fileoverview Find remark ignore files.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var fs = require('fs');
var path = require('path');
var minimatch = require('minimatch');
var debug = require('debug')('remark:cli:ignore');
var findUp = require('vfile-find-up');

/*
 * Constants.
 */

var IGNORE_NAME = '.remarkignore';
var C_BACKSLASH = '\\';
var C_SLASH = '/';
var C_EXCLAMATION = '!';
var CD = './';
var EMPTY = '';

var defaults = [
    'node_modules/'
];

/*
 * Methods.
 */

var read = fs.readFileSync;
var resolve = path.resolve;
var has = Object.prototype.hasOwnProperty;

/**
 * Check if `file` matches `pattern`.
 *
 * @example
 *   match('baz.md', '*.md'); // true
 *
 * @param {string} filePath - File location.
 * @param {string} pattern - Glob pattern.
 * @return {boolean} - Whether `file` matches `pattern`.
 */
function match(filePath, pattern) {
    return minimatch(filePath, pattern) ||
        minimatch(filePath, pattern + '/**');
}

/**
 * Check if a pattern-like line is an applicable pattern.
 *
 * @example
 *   isApplicable('node_modules'); // true
 *   isApplicable(' #node_modules'); // false
 *
 * @param {string} value - Line to check.
 * @return {boolean} - Whether `value` is an applicable
 *   pattern.
 */
function isApplicable(value) {
    var line = value && value.trim();

    return line && line.length && line.charAt(0) !== '#';
}

/**
 * Parse an ignore file.
 *
 * @example
 *   var ignore = load('~/.remarkignore');
 *
 * @throws {Error} - Throws when `filePath` is not found.
 * @param {string} filePath - File location.
 * @return {Object} - List of applicable patterns.
 */
function load(filePath) {
    var ignore = [];

    if (filePath) {
        try {
            ignore = read(filePath, 'utf8');

            ignore = ignore.split(/\r?\n/).filter(isApplicable);
        } catch (exception) {
            exception.message = 'Cannot read ignore file: ' +
                filePath + '\n' + 'Error: ' + exception.message;

            throw exception;
        }
    }

    return ignore;
}

/**
 * Find ignore-patterns.
 *
 * @example
 *   var ignore = new Ignore({
 *     'file': '.gitignore'
 *   });
 *
 * @constructor
 * @class {Ignore}
 * @param {Object?} [options] - Configuration.
 */
function Ignore(options) {
    var self = this;
    var settings = options || {};
    var file = settings.file;

    self.cache = {};
    self.cwd = options.cwd || process.cwd();

    self.detectIgnore = settings.detectIgnore;

    if (file) {
        debug('Using command line ignore `' + file + '`');

        self.cliIgnore = load(resolve(self.cwd, file));
    }
}

/**
 * Get patterns belonging to `filePath`.
 *
 * @example
 *   var ignore = new Ignore();
 *   var patterns = ignore();
 *
 * @this {Ignore}
 * @param {Function} callback - Invoked with an array of
 *   ignored paths.
 */
function loadPatterns(callback) {
    var self = this;
    var directory = self.cwd;
    var ignore = self.cache[directory];

    debug('Constructing ignore for `' + directory + '`');

    if (self.cliIgnore) {
        debug('Using ignore from CLI');

        ignore = self.cliIgnore.concat(self.defaults);
    } else if (!self.detectIgnore) {
        ignore = self.defaults;
    } else if (has.call(self.cache, directory)) {
        debug('Using ignore from cache');

        ignore = self.cache[directory];
    } else {
        findUp.one([IGNORE_NAME], directory, function (err, file) {
            var result;

            if (err) {
                callback(err);
                return;
            }

            try {
                result = load(file && file.filePath());
            } catch (err) {
                callback(err);
                return;
            }

            ignore = (result ? result : []).concat(self.defaults);

            self.patterns = self.cache[directory] = ignore;

            callback(null, ignore);
        });

        ignore = null;
    }

    if (ignore) {
        self.patterns = ignore;

        callback(null);
    }
}

/**
 * Check whether `filePath` should be ignored based on
 * the given `patterns`.
 *
 * @example
 *   ignore.shouldIgnore('readme.md'); // false
 *
 * @this {Ignore} ignore - Context.
 * @param {string} filePath - File-path to check.
 * @return {boolean} - whether `filePath` should be ignored
 *   based on the given `patterns`.
 */
function shouldIgnore(filePath) {
    var normalized = filePath
        .replace(C_BACKSLASH, C_SLASH)
        .replace(CD, EMPTY);

    return this.patterns.reduce(function (isIgnored, pattern) {
        var isNegated = pattern.charAt(0) === C_EXCLAMATION;

        if (isNegated) {
            pattern = pattern.slice(1);
        }

        if (pattern.indexOf(CD) === 0) {
            pattern = pattern.slice(CD.length);
        }

        return match(normalized, pattern) ? !isNegated : isIgnored;
    }, false);
}

/*
 * Expose methods.
 */

Ignore.prototype.shouldIgnore = shouldIgnore;
Ignore.prototype.loadPatterns = loadPatterns;

/*
 * Expose defaults.
 */

Ignore.prototype.defaults = defaults;

/*
 * Expose.
 */

module.exports = Ignore;
