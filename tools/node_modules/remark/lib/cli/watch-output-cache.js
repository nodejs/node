/**
 * @author YJ Yang
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @version 3.2.2
 * @module remark:cli:watch-output-cache
 * @fileoverview Cache changed files which are also watched.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var debug = require('debug')('remark:cli:watch-output-cache');
var fs = require('fs');

/**
 * Construct a new cache.
 *
 * @example
 *   var cache = new Cache();
 *
 * @constructor
 * @class {Cache}
 */
function Cache() {
    /**
     * Map of file-path’s to files, containing changed files
     * which are also watched.
     *
     * @member {Object}
     */
    this.cache = {};

    /**
     * Number of cached files.
     *
     * @member {number}
     */
    this.length = 0;
}

/**
 * Add a VFile to the cache so that its content can later be used.
 *
 * @this {Cache}
 * @param {VFile} file - The file to add to the cache.
 */
function add(file) {
    var filePath = file.filePath();

    this.cache[filePath] = file;
    this.length++;

    debug('Add document at %s to cache', filePath);
}

/**
 * Write to all the files in the cache.
 * This function is synchronous.
 *
 * @this {Cache}
 */
function writeAll() {
    var self = this;
    var cache = self.cache;

    Object.keys(cache).forEach(function (path) {
        var file = cache[path];
        var destinationPath = file.filePath();

        debug('Writing document to `%s`', destinationPath);

        file.stored = true;

        /*
         * Chokidar’s watcher stops the process abruptly,
         * so we need to use synchronous writes here.
        */

        fs.writeFileSync(destinationPath, file.toString());
    });

    self.length = 0;
    self.cache = {};

    debug('Written all cached documents');
}

/*
 * Attach.
 */

var proto = Cache.prototype;

proto.add = add;
proto.writeAll = writeAll;

/*
 * Expose.
 */

module.exports = Cache;
