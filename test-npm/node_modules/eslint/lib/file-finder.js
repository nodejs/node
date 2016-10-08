/**
 * @fileoverview Util class to find config files.
 * @author Aliaksei Shytkin
 * @copyright 2014 Michael McLaughlin. All rights reserved.
 * @copyright 2014 Aliaksei Shytkin. All rights reserved.
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Get the entries for a directory. Including a try-catch may be detrimental to
 * function performance, so move it out here a separate function.
 * @param {string} directory The directory to search in.
 * @returns {string[]} The entries in the directory or an empty array on error.
 * @private
 */
function getDirectoryEntries(directory) {
    try {
        return fs.readdirSync(directory);
    } catch (ex) {
        return [];
    }
}

//------------------------------------------------------------------------------
// API
//------------------------------------------------------------------------------

/**
 * FileFinder
 * @constructor
 * @param {string[]} files The basename(s) of the file(s) to find.
 * @param {stirng} cwd Current working directory
 */
function FileFinder(files, cwd) {
    this.fileNames = Array.isArray(files) ? files : [files];
    this.cwd = cwd || process.cwd();
    this.cache = {};
}

/**
 * Create a hash of filenames from a directory listing
 * @param {string[]} entries Array of directory entries.
 * @param {string} directory Path to a current directory.
 * @param {string[]} supportedConfigs List of support filenames.
 * @returns {Object} Hashmap of filenames
 */
function normalizeDirectoryEntries(entries, directory, supportedConfigs) {
    var fileHash = {};
    entries.forEach(function(entry) {
        if (supportedConfigs.indexOf(entry) >= 0) {
            var resolvedEntry = path.resolve(directory, entry);
            if (fs.statSync(resolvedEntry).isFile()) {
                fileHash[entry] = resolvedEntry;
            }
        }
    });
    return fileHash;
}

/**
 * Find one instance of a specified file name in directory or in a parent directory.
 * Cache the results.
 * Does not check if a matching directory entry is a file, and intentionally
 * only searches for the first file name in this.fileNames.
 * Is currently used by lib/ignored_paths.js to find an .eslintignore file.
 * @param {string} directory The directory to start the search from.
 * @returns {string} Path of the file found, or an empty string if not found.
 */
FileFinder.prototype.findInDirectoryOrParents = function(directory) {
    var cache = this.cache,
        child,
        dirs,
        filePath,
        i,
        names,
        searched;

    if (!directory) {
        directory = this.cwd;
    }

    if (cache.hasOwnProperty(directory)) {
        return cache[directory];
    }

    dirs = [];
    searched = 0;
    names = this.fileNames;

    (function() {
        while (directory !== child) {
            dirs[searched++] = directory;
            var filesMap = normalizeDirectoryEntries(getDirectoryEntries(directory), directory, names);
            if (Object.keys(filesMap).length) {
                for (var k = 0; k < names.length; k++) {
                    if (filesMap[names[k]]) {
                        filePath = filesMap[names[k]];
                        return;
                    }
                }
            }

            child = directory;

            // Assign parent directory to directory.
            directory = path.dirname(directory);
        }
    }());

    for (i = 0; i < searched; i++) {
        cache[dirs[i]] = filePath;
    }

    return filePath || String();
};

/**
 * Find all instances of files with the specified file names, in directory and
 * parent directories. Cache the results.
 * Does not check if a matching directory entry is a file.
 * Searches for all the file names in this.fileNames.
 * Is currently used by lib/config.js to find .eslintrc and package.json files.
 * @param  {string} directory The directory to start the search from.
 * @returns {string[]} The file paths found.
 */
FileFinder.prototype.findAllInDirectoryAndParents = function(directory) {
    var cache = this.cache,
        child,
        dirs,
        fileNames,
        filePath,
        i,
        j,
        searched;

    if (!directory) {
        directory = this.cwd;
    }

    if (cache.hasOwnProperty(directory)) {
        return cache[directory];
    }

    dirs = [];
    searched = 0;
    fileNames = this.fileNames;

    do {
        dirs[searched++] = directory;
        cache[directory] = [];

        var filesMap = normalizeDirectoryEntries(getDirectoryEntries(directory), directory, fileNames);

        if (Object.keys(filesMap).length) {
            for (var k = 0; k < fileNames.length; k++) {

                if (filesMap[fileNames[k]]) {
                    filePath = filesMap[fileNames[k]];

                    // Add the file path to the cache of each directory searched.
                    for (j = 0; j < searched; j++) {
                        cache[dirs[j]].push(filePath);
                    }

                    break;
                }
            }
        }
        child = directory;

        // Assign parent directory to directory.
        directory = path.dirname(directory);

        if (directory === child) {
            return cache[dirs[0]];
        }
    } while (!cache.hasOwnProperty(directory));

    // Add what has been cached previously to the cache of each directory searched.
    for (i = 0; i < searched; i++) {
        dirs.push.apply(cache[dirs[i]], cache[directory]);
    }

    return cache[dirs[0]];
};

module.exports = FileFinder;
