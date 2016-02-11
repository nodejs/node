/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-set-pipeline:transform
 * @version 3.2.2
 * @fileoverview Transform all files.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var FileSet = require('../file-set');

/**
 * Transform all files.
 *
 * @example
 *   var context = new CLI(['.', '-u toc']);
 *   transform(context, console.log);
 *
 * @param {Object} context - Context object.
 * @param {function(Error?)} done - Completion handler.
 */
function transform(context, done) {
    var fileSet = new FileSet(context);

    fileSet.done = done;

    context.files.forEach(fileSet.add, fileSet);

    context.fileSet = fileSet;
}

/*
 * Expose.
 */

module.exports = transform;
