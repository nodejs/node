'use strict';

/*
 * Dependencies.
 */

var fs = require('fs');
var toVFile = require('./lib/to-vfile.js');

/*
 * Methods.
 */

var read = fs.readFile;
var readSync = fs.readFileSync;

/**
 * Async callback.
 *
 * @callback toVFile~callback
 * @param {Error} err - Error from reading `filePath`.
 * @param {VFile} file - Virtual file.
 */

/**
 * Create a virtual file from `filePath` and fill it with
 * the actual contents at `filePath`.
 *
 * @param {string} filePath - Path to file.
 * @param {toVFile~callback} callback - Callback.
 */
function async(filePath, callback) {
    var file = toVFile(filePath);

    read(filePath, 'utf-8', function (err, res) {
        if (err) {
            callback(err);
        } else {
            file.contents = res;

            callback(null, file);
        }
    });
}

/**
 * Create a virtual file from `filePath` and fill it with
 * the actual contents at `filePath` (synchroneously).
 *
 * @param {string} filePath - Path to file.
 * @throws {Error} err - When reading `filePath` fails.
 * @return {VFile} Virtual file.
 */
function sync(filePath) {
    var file = toVFile(filePath);

    file.contents = readSync(filePath, 'utf-8');

    return file;
}

/*
 * Expose.
 */

toVFile.read = async;
toVFile.readSync = sync;

module.exports = toVFile;
