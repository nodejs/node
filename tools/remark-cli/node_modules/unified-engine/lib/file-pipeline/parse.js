/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:parse
 * @fileoverview Parse a file into an AST.
 */

'use strict';

/* Dependencies. */
var debug = require('debug')('unified-engine:file-pipeline:parse');
var stats = require('vfile-statistics');
var json = require('parse-json');

/* Expose. */
module.exports = parse;

/**
 * Fill a file with an ast.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 */
function parse(context, file) {
  var message;

  if (stats(file).fatal) {
    return;
  }

  if (context.treeIn) {
    debug('Not parsing already parsed document');

    try {
      context.tree = json(file.toString());
    } catch (err) {
      err.message = 'Cannot read file as JSON\n' + err.message;

      message = file.message(err);
      message.fatal = true;
    }

    /* Add the preferred extension to ensure the file,
     * when compiled, is correctly recognized. */
    file.extname = '.' + context.extensions[0];
    file.contents = '';

    return;
  }

  debug('Parsing `%s` with `%j`', file.path, context.settings);

  context.tree = context.processor.parse(file, context.settings);

  debug('Parsed document');
}
