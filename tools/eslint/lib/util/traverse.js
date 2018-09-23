/**
 * @fileoverview Simple directory traversal logic.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path"),
    debug = require("debug");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

debug = debug("eslint:traverse");

/**
 * Walks a path recursively calling the callback on each file.
 * @param {string} name The file or directory path.
 * @param {string[]} extensions The file extensions that should cause the callback
 *      to be called.
 * @param {Function} exclude The function to check if file/path should be excluded.
 * @param {Function} callback The function to call on each file.
 * @returns {void}
 * @private
 */
function walk(name, extensions, exclude, callback) {

    var stat, basename;

    stat = fs.statSync(name);

    function traverse(dir, stack) {
        stack.push(dir);

        fs.readdirSync(path.join.apply(path, stack)).forEach(function(file) {
            var filePath, fileStat;

            // skip all hidded things (dirs, files, links)
            if (file[0] === ".") {
                return;
            }

            filePath = path.join.apply(path, stack.concat([file]));
            fileStat = fs.statSync(filePath);

            // if this file or directory is excluded from linting, skip over it.
            if (exclude && exclude(filePath)) {
                // console.log("Ignoring " + filePath);
                debug("Ignoring " + filePath);
                return;
            }

            // only call callback for files with correct extensions
            if (fileStat.isFile() && extensions.indexOf(path.extname(filePath)) > -1) {
                callback(filePath);
            } else if (fileStat.isDirectory()) {
                traverse(file, stack);
            }
        });
        stack.pop();
    }

    basename = path.basename(name);

    // don't ignore cases like 'eslint ./'
    if ((basename !== "." && basename !== ".." && basename[0] === ".") ||
        (exclude && exclude(name))) {

        debug("Ignoring " + name);
        return;
    }

    // always call callback for any files that are passed on the command line
    if (stat.isFile()) {
        callback(name);
    } else {
        traverse(name, []);
    }
}

/**
 * Traverses multiple directories and calls a callback on each file.
 * @param {Object} options The option for the traversal.
 * param {string[]} options.files An array of file and directory paths to traverse.
 * param {Function} options.exclude The function to check if file/path should be excluded.
 * @param {Function} callback A function to call for each file.
 * @returns {void}
 */
module.exports = function traverse(options, callback) {

    var files = options.files,
        exclude = options.exclude,
        extensions = options.extensions;

    files.forEach(function(file) {
        walk(file, extensions, exclude, callback);
    });

};
