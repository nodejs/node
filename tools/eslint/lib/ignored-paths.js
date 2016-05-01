/**
 * @fileoverview Responsible for loading ignore config files and managing ignore patterns
 * @author Jonathan Rajavuori
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var lodash = require("lodash"),
    fs = require("fs"),
    path = require("path"),
    debug = require("debug"),
    ignore = require("ignore"),
    pathUtil = require("./util/path-util");

debug = debug("eslint:ignored-paths");


//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var ESLINT_IGNORE_FILENAME = ".eslintignore";
var DEFAULT_IGNORE_PATTERNS = [
    "/node_modules/*",
    "/bower_components/*"
];
var DEFAULT_OPTIONS = {
    dotfiles: false,
    cwd: process.cwd()
};


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------


/**
 * Find an ignore file in the current directory.
 * @param {stirng} cwd Current working directory
 * @returns {string} Path of ignore file or an empty string.
 */
function findIgnoreFile(cwd) {
    cwd = cwd || DEFAULT_OPTIONS.cwd;

    var ignoreFilePath = path.resolve(cwd, ESLINT_IGNORE_FILENAME);

    return fs.existsSync(ignoreFilePath) ? ignoreFilePath : "";
}

/**
 * Merge options with defaults
 * @param {object} options Options to merge with DEFAULT_OPTIONS constant
 * @returns {object} Merged options
 */
function mergeDefaultOptions(options) {
    options = (options || {});
    return lodash.assign({}, DEFAULT_OPTIONS, options);
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * IgnoredPaths
 * @constructor
 * @class IgnoredPaths
 * @param {Object} options object containing 'ignore', 'ignorePath' and 'patterns' properties
 */
function IgnoredPaths(options) {

    options = mergeDefaultOptions(options);

    /**
     * add pattern to node-ignore instance
     * @param {object} ig, instance of node-ignore
     * @param {string} pattern, pattern do add to ig
     * @returns {array} raw ignore rules
     */
    function addPattern(ig, pattern) {
        return ig.addPattern(pattern);
    }

    /**
     * add ignore file to node-ignore instance
     * @param {object} ig, instance of node-ignore
     * @param {string} filepath, file to add to ig
     * @returns {array} raw ignore rules
     */
    function addIgnoreFile(ig, filepath) {
        ig.ignoreFiles.push(filepath);
        return ig.add(fs.readFileSync(filepath, "utf8"));
    }

    this.defaultPatterns = DEFAULT_IGNORE_PATTERNS.concat(options.patterns || []);
    this.baseDir = options.cwd;

    this.ig = {
        custom: ignore(),
        default: ignore()
    };

    // Add a way to keep track of ignored files.  This was present in node-ignore
    // 2.x, but dropped for now as of 3.0.10.
    this.ig.custom.ignoreFiles = [];
    this.ig.default.ignoreFiles = [];

    if (options.dotfiles !== true) {

        /*
         * ignore files beginning with a dot, but not files in a parent or
         * ancestor directory (which in relative format will begin with `../`).
         */
        addPattern(this.ig.default, [".*", "!../"]);
    }

    addPattern(this.ig.default, this.defaultPatterns);

    if (options.ignore !== false) {
        var ignorePath;

        if (options.ignorePattern) {
            addPattern(this.ig.custom, options.ignorePattern);
            addPattern(this.ig.default, options.ignorePattern);
        }

        if (options.ignorePath) {
            debug("Using specific ignore file");

            try {
                fs.statSync(options.ignorePath);
                ignorePath = options.ignorePath;
            } catch (e) {
                e.message = "Cannot read ignore file: " + options.ignorePath + "\nError: " + e.message;
                throw e;
            }
        } else {
            debug("Looking for ignore file in " + options.cwd);
            ignorePath = findIgnoreFile(options.cwd);

            try {
                fs.statSync(ignorePath);
                debug("Loaded ignore file " + ignorePath);
            } catch (e) {
                debug("Could not find ignore file in cwd");
                this.options = options;
            }
        }

        if (ignorePath) {
            debug("Adding " + ignorePath);
            this.baseDir = path.dirname(path.resolve(options.cwd, ignorePath));
            addIgnoreFile(this.ig.custom, ignorePath);
            addIgnoreFile(this.ig.default, ignorePath);
        }

    }

    this.options = options;

}

/**
 * Determine whether a file path is included in the default or custom ignore patterns
 * @param {string} filepath Path to check
 * @param {string} [category=null] check 'default', 'custom' or both (null)
 * @returns {boolean} true if the file path matches one or more patterns, false otherwise
 */
IgnoredPaths.prototype.contains = function(filepath, category) {

    var result = false;
    var absolutePath = path.resolve(this.options.cwd, filepath);
    var relativePath = pathUtil.getRelativePath(absolutePath, this.options.cwd);

    if ((typeof category === "undefined") || (category === "default")) {
        result = result || (this.ig.default.filter([relativePath]).length === 0);
    }

    if ((typeof category === "undefined") || (category === "custom")) {
        result = result || (this.ig.custom.filter([relativePath]).length === 0);
    }

    return result;

};

module.exports = IgnoredPaths;
