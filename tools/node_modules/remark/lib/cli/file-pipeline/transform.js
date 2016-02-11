/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-pipeline:transform
 * @version 3.2.2
 * @fileoverview Transform an AST associated with a file.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var debug = require('debug')('remark:cli:file-pipeline:transform');

/**
 * Transform the `ast` associated with a file with
 * configured plug-ins.
 *
 * @example
 *   var file = new File();
 *
 *   file.namespace('mdast').tree = {
 *     'type': 'paragraph',
 *     'children': [{
 *       'type': 'text',
 *       'value': 'Foo.'
 *     }]
 *   };
 *
 *   transform({'file': file}, console.log);
 *
 * @param {Object} context - Context object.
 * @param {function(Error?)} done - Completion handler.
 */
function transform(context, done) {
    var file = context.file;

    if (file.hasFailed()) {
        done();
        return;
    }

    debug('Transforming document', file.filePath());

    context.processor.run(file.namespace('mdast').tree, file, function (err) {
        debug('Transformed document (error: %s)', err);

        done(err);
    });
}

/*
 * Expose.
 */

module.exports = transform;
