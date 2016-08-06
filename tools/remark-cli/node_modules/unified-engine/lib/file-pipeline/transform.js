/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:transform
 * @fileoverview Transform an AST associated with a file.
 */

'use strict';

/* Dependencies. */
var debug = require('debug')('unified-engine:file-pipeline:transform');
var stats = require('vfile-statistics');

/* Expose. */
module.exports = transform;

/**
 * Transform the `ast` associated with a file with
 * configured plug-ins.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 * @param {FileSet} fileSet - Set.
 * @param {function(Error?)} done - Completion handler.
 */
function transform(context, file, fileSet, done) {
  if (stats(file).fatal) {
    done();
    return;
  }

  debug('Transforming document `%s`', file.path);

  context.processor.run(context.tree, file, function (err, node) {
    debug('Transformed document (error: %s)', err);

    context.tree = node;

    done(err);
  });
}
