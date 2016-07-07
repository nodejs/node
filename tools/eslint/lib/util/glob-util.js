/**
 * @fileoverview Utilities for working with globs and the filesystem.
 * @author Ian VanSchooten
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug"),
    fs = require("fs"),
    path = require("path"),
    glob = require("glob"),
    shell = require("shelljs"),

    pathUtil = require("./path-util"),
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
 * @param {object}   [options]                    An options object
 * @param {string[]} [options.extensions=[".js"]] An array of accepted extensions
 * @param {string}   [options.cwd=process.cwd()]  The cwd to use to resolve relative pathnames
 * @returns {Function} A function that takes a pathname and returns a glob that
 *                     matches all files with the provided extensions if
 *                     pathname is a directory.
 */
function processPath(options) {
    var cwd = (options && options.cwd) || process.cwd();
    var extensions = (options && options.extensions) || [".js"];

    extensions = extensions.map(function(ext) {
        return ext.charAt(0) === "." ? ext.substr(1) : ext;
    });

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
        var resolvedPath = path.resolve(cwd, pathname);

        if (shell.test("-d", resolvedPath)) {
            newPath = pathname.replace(/[\/\\]$/, "") + suffix;
        }

        return pathUtil.convertPathToPosix(newPath);
    };
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Resolves any directory patterns into glob-based patterns for easier handling.
 * @param   {string[]} patterns    File patterns (such as passed on the command line).
 * @param   {Object} options       An options object.
 * @returns {string[]} The equivalent glob patterns and filepath strings.
 */
function resolveFileGlobPatterns(patterns, options) {

    var processPathExtensions = processPath(options);

    return patterns.map(processPathExtensions);
}

/**
 * Build a list of absolute filesnames on which ESLint will act.
 * Ignored files are excluded from the results, as are duplicates.
 *
 * @param   {string[]} globPatterns            Glob patterns.
 * @param   {Object}   [options]               An options object.
 * @param   {string}   [options.cwd]           CWD (considered for relative filenames)
 * @param   {boolean}  [options.ignore]        False disables use of .eslintignore.
 * @param   {string}   [options.ignorePath]    The ignore file to use instead of .eslintignore.
 * @param   {string}   [options.ignorePattern] A pattern of files to ignore.
 * @returns {string[]} Resolved absolute filenames.
 */
function listFilesToProcess(globPatterns, options) {
    var ignoredPaths,
        files = [],
        added = {},
        globOptions;

    var cwd = (options && options.cwd) || process.cwd();

    /**
     * Executes the linter on a file defined by the `filename`. Skips
     * unsupported file extensions and any files that are already linted.
     * @param {string} filename The file to be processed
     * @param {boolean} shouldWarnIgnored Whether or not a report should be made if
     *                                    the file is ignored
     * @returns {void}
     */
    function addFile(filename, shouldWarnIgnored) {
        var ignored = false;
        var isSilentlyIgnored;

        if (ignoredPaths.contains(filename, "default")) {
            ignored = (options.ignore !== false) && shouldWarnIgnored;
            isSilentlyIgnored = !shouldWarnIgnored;
        }

        if (options.ignore !== false) {
            if (ignoredPaths.contains(filename, "custom")) {
                if (shouldWarnIgnored) {
                    ignored = true;
                } else {
                    isSilentlyIgnored = true;
                }
            }
        }

        if (isSilentlyIgnored && !ignored) {
            return;
        }

        if (added[filename]) {
            return;
        }
        files.push({filename: filename, ignored: ignored});
        added[filename] = true;
    }

    options = options || { ignore: true, dotfiles: true };
    ignoredPaths = new IgnoredPaths(options);
    globOptions = {
        nodir: true,
        cwd: cwd,
        ignore: ignoredPaths.getIgnoredFoldersGlobPatterns()
    };

    debug("Creating list of files to process.");
    globPatterns.forEach(function(pattern) {
        var file = path.resolve(cwd, pattern);

        if (shell.test("-f", file)) {
            addFile(fs.realpathSync(file), !shell.test("-d", file));
        } else {
            glob.sync(pattern, globOptions).forEach(function(globMatch) {
                addFile(path.resolve(cwd, globMatch), false);
            });
        }
    });

    return files;
}

module.exports = {
    resolveFileGlobPatterns: resolveFileGlobPatterns,
    listFilesToProcess: listFilesToProcess
};
