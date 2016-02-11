/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-pipeline:queue
 * @version 3.2.2
 * @fileoverview Queue all files which came this far.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var debug = require('debug')('remark:cli:file-pipeline:queue');

/**
 * Queue all files which came this far.
 * When the last file gets here, run the file-set pipeline
 * and flush the queue.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *   var file = new File();
 *
 *   file.sourcePath = '~/foo/bar.md';
 *   fileSet.add(file);
 *
 *   queue({'file': file, 'fileSet': fileSet}, console.log);
 *
 * @param {Object} context - Context object.
 * @param {function(Error?)} done - Completion handler.
 */
function queue(context, done) {
    var fileSet = context.fileSet;
    var original = context.file.sourcePath;
    var complete = true;
    var map = fileSet.complete;

    if (!map) {
        map = fileSet.complete = {};
    }

    debug('Queueing %s', original);

    map[original] = done;

    fileSet.valueOf().forEach(function (file) {
        var key = file.sourcePath;

        if (file.hasFailed()) {
            return;
        }

        if (typeof map[key] !== 'function') {
            debug('Interupting flush as %s is not finished', key);
            complete = false;
        } else {
            debug('Can flush for `%s`', key);
        }
    });

    if (!complete) {
        debug('Not flushing');
        return;
    }

    fileSet.pipeline.run(fileSet, function (err) {
        debug('Flushing');
        /*
         * Flush.
         */

        for (original in map) {
            map[original](err);
        }
    });
}

/*
 * Expose.
 */

module.exports = queue;
