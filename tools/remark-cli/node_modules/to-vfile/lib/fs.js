/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module to-vfile
 * @fileoverview Read and write virtual files from the file-system.
 */

'use strict';

/* Dependencies. */
var fs = require('fs');
var vfile = require('./core');

/* Expose. */
module.exports = vfile;

vfile.read = read;
vfile.readSync = readSync;
vfile.write = write;
vfile.writeSync = writeSync;

/**
 * Create a virtual file and read it in, asynchronously.
 *
 * @param {*} description - File description.
 * @param {Object|string} [options] - `fs.readFile` options.
 * @param {Function} callback - Callback.
 */
function read(description, options, callback) {
  var file = vfile(description);

  if (!callback) {
    callback = options;
    options = null;
  }

  fs.readFile(file.path, options, function (err, res) {
    if (err) {
      callback(err);
    } else {
      file.contents = res;
      callback(null, file);
    }
  });
}

/**
 * Create a virtual file and read it in, synchronously.
 *
 * @param {*} description - File description.
 * @param {Object|string} [options] - `fs.readFile` options.
 */
function readSync(description, options) {
  var file = vfile(description);
  file.contents = fs.readFileSync(file.path, options);
  return file;
}

/**
 * Create a virtual file and write it out, asynchronously.
 *
 * @param {*} description - File description.
 * @param {Object|string} [options] - `fs.writeFile` options.
 * @param {Function} callback - Callback.
 */
function write(description, options, callback) {
  var file = vfile(description);

  /* Weird, right? Otherwise `fs` doesn’t accept it. */
  if (!callback) {
    callback = options;
    options = undefined;
  }

  fs.writeFile(file.path, String(file), options, callback);
}

/**
 * Create a virtual file and write it out, synchronously.
 *
 * @param {*} description - File description.
 * @param {Object|string} [options] - `fs.writeFile` options.
 */
function writeSync(description, options) {
  var file = vfile(description);
  fs.writeFileSync(file.path, String(file), options);
}
