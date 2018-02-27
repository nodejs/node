/**
 * @fileoverview Responsible for loading ignore config files and managing ignore patterns
 * @author Jonathan Rajavuori
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const fs = require("fs"),
    path = require("path"),
    ignore = require("ignore"),
    pathUtil = require("./util/path-util");

const debug = require("debug")("eslint:ignored-paths");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

const ESLINT_IGNORE_FILENAME = ".eslintignore";

/**
 * Adds `"*"` at the end of `"node_modules/"`,
 * so that subtle directories could be re-included by .gitignore patterns
 * such as `"!node_modules/should_not_ignored"`
 */
const DEFAULT_IGNORE_DIRS = [
    "/node_modules/*",
    "/bower_components/*"
];
const DEFAULT_OPTIONS = {
    dotfiles: false,
    cwd: process.cwd()
};

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Find a file in the current directory.
 * @param {string} cwd Current working directory
 * @param {string} name File name
 * @returns {string} Path of ignore file or an empty string.
 */
function findFile(cwd, name) {
    const ignoreFilePath = path.resolve(cwd, name);

    return fs.existsSync(ignoreFilePath) && fs.statSync(ignoreFilePath).isFile() ? ignoreFilePath : "";
}

/**
 * Find an ignore file in the current directory.
 * @param {string} cwd Current working directory
 * @returns {string} Path of ignore file or an empty string.
 */
function findIgnoreFile(cwd) {
    return findFile(cwd, ESLINT_IGNORE_FILENAME);
}

/**
 * Find an package.json file in the current directory.
 * @param {string} cwd Current working directory
 * @returns {string} Path of package.json file or an empty string.
 */
function findPackageJSONFile(cwd) {
    return findFile(cwd, "package.json");
}

/**
 * Merge options with defaults
 * @param {Object} options Options to merge with DEFAULT_OPTIONS constant
 * @returns {Object} Merged options
 */
function mergeDefaultOptions(options) {
    options = (options || {});
    return Object.assign({}, DEFAULT_OPTIONS, options);
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * IgnoredPaths class
 */
class IgnoredPaths {

    /**
     * @param {Object} options object containing 'ignore', 'ignorePath' and 'patterns' properties
     */
    constructor(options) {
        options = mergeDefaultOptions(options);
        this.cache = {};

        /**
         * add pattern to node-ignore instance
         * @param {Object} ig, instance of node-ignore
         * @param {string} pattern, pattern do add to ig
         * @returns {array} raw ignore rules
         */
        function addPattern(ig, pattern) {
            return ig.addPattern(pattern);
        }

        this.defaultPatterns = [].concat(DEFAULT_IGNORE_DIRS, options.patterns || []);
        this.baseDir = options.cwd;

        this.ig = {
            custom: ignore(),
            default: ignore()
        };

        /*
         * Add a way to keep track of ignored files.  This was present in node-ignore
         * 2.x, but dropped for now as of 3.0.10.
         */
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
            let ignorePath;

            if (options.ignorePath) {
                debug("Using specific ignore file");

                try {
                    fs.statSync(options.ignorePath);
                    ignorePath = options.ignorePath;
                } catch (e) {
                    e.message = `Cannot read ignore file: ${options.ignorePath}\nError: ${e.message}`;
                    throw e;
                }
            } else {
                debug(`Looking for ignore file in ${options.cwd}`);
                ignorePath = findIgnoreFile(options.cwd);

                try {
                    fs.statSync(ignorePath);
                    debug(`Loaded ignore file ${ignorePath}`);
                } catch (e) {
                    debug("Could not find ignore file in cwd");
                    this.options = options;
                }
            }

            if (ignorePath) {
                debug(`Adding ${ignorePath}`);
                this.baseDir = path.dirname(path.resolve(options.cwd, ignorePath));
                this.addIgnoreFile(this.ig.custom, ignorePath);
                this.addIgnoreFile(this.ig.default, ignorePath);
            } else {
                try {

                    // if the ignoreFile does not exist, check package.json for eslintIgnore
                    const packageJSONPath = findPackageJSONFile(options.cwd);

                    if (packageJSONPath) {
                        let packageJSONOptions;

                        try {
                            packageJSONOptions = JSON.parse(fs.readFileSync(packageJSONPath, "utf8"));
                        } catch (e) {
                            debug("Could not read package.json file to check eslintIgnore property");
                            throw e;
                        }

                        if (packageJSONOptions.eslintIgnore) {
                            if (Array.isArray(packageJSONOptions.eslintIgnore)) {
                                packageJSONOptions.eslintIgnore.forEach(pattern => {
                                    addPattern(this.ig.custom, pattern);
                                    addPattern(this.ig.default, pattern);
                                });
                            } else {
                                throw new TypeError("Package.json eslintIgnore property requires an array of paths");
                            }
                        }
                    }
                } catch (e) {
                    debug("Could not find package.json to check eslintIgnore property");
                    throw e;
                }
            }

            if (options.ignorePattern) {
                addPattern(this.ig.custom, options.ignorePattern);
                addPattern(this.ig.default, options.ignorePattern);
            }
        }

        this.options = options;
    }

    /**
     * read ignore filepath
     * @param {string} filePath, file to add to ig
     * @returns {array} raw ignore rules
     */
    readIgnoreFile(filePath) {
        if (typeof this.cache[filePath] === "undefined") {
            this.cache[filePath] = fs.readFileSync(filePath, "utf8");
        }
        return this.cache[filePath];
    }

    /**
     * add ignore file to node-ignore instance
     * @param {Object} ig, instance of node-ignore
     * @param {string} filePath, file to add to ig
     * @returns {array} raw ignore rules
     */
    addIgnoreFile(ig, filePath) {
        ig.ignoreFiles.push(filePath);
        return ig.add(this.readIgnoreFile(filePath));
    }

    /**
     * Determine whether a file path is included in the default or custom ignore patterns
     * @param {string} filepath Path to check
     * @param {string} [category=null] check 'default', 'custom' or both (null)
     * @returns {boolean} true if the file path matches one or more patterns, false otherwise
     */
    contains(filepath, category) {

        let result = false;
        const absolutePath = path.resolve(this.options.cwd, filepath);
        const relativePath = pathUtil.getRelativePath(absolutePath, this.baseDir);

        if ((typeof category === "undefined") || (category === "default")) {
            result = result || (this.ig.default.filter([relativePath]).length === 0);
        }

        if ((typeof category === "undefined") || (category === "custom")) {
            result = result || (this.ig.custom.filter([relativePath]).length === 0);
        }

        return result;

    }

    /**
     * Returns a list of dir patterns for glob to ignore
     * @returns {function()} method to check whether a folder should be ignored by glob.
     */
    getIgnoredFoldersGlobChecker() {

        const ig = ignore().add(DEFAULT_IGNORE_DIRS);

        if (this.options.dotfiles !== true) {

            // Ignore hidden folders.  (This cannot be ".*", or else it's not possible to unignore hidden files)
            ig.add([".*/*", "!../"]);
        }

        if (this.options.ignore) {
            ig.add(this.ig.custom);
        }

        const filter = ig.createFilter();

        const base = this.baseDir;

        return function(absolutePath) {
            const relative = pathUtil.getRelativePath(absolutePath, base);

            if (!relative) {
                return false;
            }

            return !filter(relative);
        };
    }
}

module.exports = IgnoredPaths;
