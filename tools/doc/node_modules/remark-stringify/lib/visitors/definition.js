/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:definition
 * @fileoverview Stringify a definition.
 */

'use strict';

/* Dependencies. */
var uri = require('../util/enclose-uri');
var title = require('../util/enclose-title');

/* Expose. */
module.exports = definition;

/**
 * Stringify an URL definition.
 *
 * Is smart about enclosing `url` (see `encloseURI()`) and
 * `title` (see `encloseTitle()`).
 *
 *    [foo]: <foo at bar dot com> 'An "example" e-mail'
 *
 * @param {Object} node - `definition` node.
 * @return {string} - Markdown definition.
 */
function definition(node) {
  var content = uri(node.url);

  if (node.title) {
    content += ' ' + title(node.title);
  }

  return '[' + node.identifier + ']: ' + content;
}
