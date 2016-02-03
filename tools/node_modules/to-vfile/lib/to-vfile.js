'use strict';

/*
 * Dependencies.
 */

var path = require('path');
var VFile = require('vfile');

/*
 * Methods.
 */

var extname = path.extname;
var basename = path.basename;
var dirname = path.dirname;

/**
 * Create a virtual file from `filePath`.
 *
 * @param {string} filePath - Path to file.
 * @return {VFile} Virtual file.
 */
function toFile(filePath) {
    var extension = extname(filePath);

    return new VFile({
        'directory': dirname(filePath),
        'filename': basename(filePath, extension),
        'extension': extension.slice(1)
    });
}

/*
 * Expose.
 */

module.exports = toFile;
