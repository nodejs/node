/**
 * @fileoverview Common helpers for operations on filenames and paths
 * @author Ian VanSchooten
 * @copyright 2016 Ian VanSchooten. All rights reserved.
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var path = require("path"),
    isAbsolute = require("path-is-absolute");

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

/**
 * Replace Windows with posix style paths
 *
 * @param {string} filepath   Path to convert
 * @returns {string}          Converted filepath
 */
function convertPathToPosix(filepath) {
    var normalizedFilepath = path.normalize(filepath);
    var posixFilepath = normalizedFilepath.replace(/\\/g, "/");

    return posixFilepath;
}

/**
 * Converts an absolute filepath to a relative path from a given base path
 *
 * For example, if the filepath is `/my/awesome/project/foo.bar`,
 * and the base directory is `/my/awesome/project/`,
 * then this function should return `foo.bar`.
 *
 * path.relative() does something similar, but it requires a baseDir (`from` argument).
 * This function makes it optional and just removes a leading slash if the baseDir is not given.
 *
 * It does not take into account symlinks (for now).
 *
 * @param {string} filepath  Path to convert to relative path.  If already relative,
 *                           it will be assumed to be relative to process.cwd(),
 *                           converted to absolute, and then processed.
 * @param {string} [baseDir] Absolute base directory to resolve the filepath from.
 *                           If not provided, all this function will do is remove
 *                           a leading slash.
 * @returns {string} Relative filepath
 */
function getRelativePath(filepath, baseDir) {
    var relativePath;

    if (!isAbsolute(filepath)) {
        filepath = path.resolve(filepath);
    }
    if (baseDir) {
        if (!isAbsolute(baseDir)) {
            throw new Error("baseDir should be an absolute path");
        }
        relativePath = path.relative(baseDir, filepath);
    } else {
        relativePath = filepath.replace(/^\//, "");
    }
    return relativePath;
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    convertPathToPosix: convertPathToPosix,
    getRelativePath: getRelativePath
};
