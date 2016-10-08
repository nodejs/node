/**
 * @fileoverview Utilities for working with globs and the filesystem.
 * @author Ian VanSchooten
 * @copyright 2015 Ian VanSchooten. All rights reserved.
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug"),
    fs = require("fs"),
    glob = require("glob"),
    shell = require("shelljs"),

    IgnoredPaths = require("../ignored-paths");

debug = debug("eslint:glob-util");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks if a provided path is a directory and returns a glob string matching
 * all files under that directory if so, the path itself otherwise.
 *
 * Reason for this is that `glob` needs `/**` to collect all the files under a
 * directory where as our previous implementation without `glob` simply walked
 * a directory that is passed. So this is to maintain backwards compatibility.
 *
 * Also makes sure all path separators are POSIX style for `glob` compatibility.
 *
 * @param {string[]} extensions An array of accepted extensions
 * @returns {Function} A function that takes a pathname and returns a glob that
 *                     matches all files with the provided extensions if
 *                     pathname is a directory.
 */
function processPath(extensions) {
    var suffix = "/**";

    if (extensions.length === 1) {
        suffix += "/*." + extensions[0];
    } else {
        suffix += "/*.{" + extensions.join(",") + "}";
    }

    /**
     * A function that converts a directory name to a glob pattern
     *
     * @param {string} pathname The directory path to be modified
     * @returns {string} The glob path or the file path itself
     * @private
     */
    return function(pathname) {
        var newPath = pathname;

        if (shell.test("-d", pathname)) {
            newPath = pathname.replace(/[\/\\]$/, "") + suffix;
        }

        return newPath.replace(/\\/g, "/").replace(/^\.\//, "");
    };
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Resolves the patterns into glob-based patterns for easier handling.
 * @param   {string[]} patterns    File patterns (such as passed on the command line).
 * @param   {string[]} extensions  List of valid file extensions.
 * @returns {string[]} The equivalent glob patterns.
 */
function resolveFileGlobPatterns(patterns, extensions) {
    extensions = extensions || [".js"];

    extensions = extensions.map(function(ext) {
        return ext.charAt(0) === "." ? ext.substr(1) : ext;
    });

    var processPathExtensions = processPath(extensions);
    return patterns.map(processPathExtensions);
}

/**
 * Build a list of absolute filesnames on which ESLint will act.
 * Ignored files are excluded from the results, as are duplicates.
 *
 * @param   {string[]} globPatterns            Glob patterns.
 * @param   {Object}   [options]               An options object.
 * @param   {boolean}  [options.ignore]        False disables use of .eslintignore.
 * @param   {string}   [options.ignorePath]    The ignore file to use instead of .eslintignore.
 * @param   {string}   [options.ignorePattern] A pattern of files to ignore.
 * @returns {string[]} Resolved absolute filenames.
 */
function listFilesToProcess(globPatterns, options) {
    var ignoredPaths,
        ignoredPathsList,
        files = [],
        added = {},
        globOptions,
        rulesKey = "_rules";

    /**
     * Executes the linter on a file defined by the `filename`. Skips
     * unsupported file extensions and any files that are already linted.
     * @param {string} filename The file to be processed
     * @returns {void}
     */
    function addFile(filename) {
        if (ignoredPaths.contains(filename)) {
            return;
        }
        filename = fs.realpathSync(filename);
        if (added[filename]) {
            return;
        }
        files.push(filename);
        added[filename] = true;
    }

    options = options || { ignore: true, dotfiles: true };
    ignoredPaths = new IgnoredPaths(options);
    ignoredPathsList = ignoredPaths.ig.custom[rulesKey].map(function(rule) {
        return rule.pattern;
    });
    globOptions = {
        nodir: true,
        ignore: ignoredPathsList
    };

    debug("Creating list of files to process.");
    globPatterns.forEach(function(pattern) {
        if (shell.test("-f", pattern)) {
            addFile(pattern);
        } else {
            glob.sync(pattern, globOptions).forEach(addFile);
        }
    });

    return files;
}

module.exports = {
    resolveFileGlobPatterns: resolveFileGlobPatterns,
    listFilesToProcess: listFilesToProcess
};
