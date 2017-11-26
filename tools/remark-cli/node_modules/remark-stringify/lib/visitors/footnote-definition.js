/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:footnote-definition
 * @fileoverview Stringify a footnote-definition.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');

/* Expose. */
module.exports = footnoteDefinition;

/**
 * Stringify a footnote definition.
 *
 * @param {Object} node - `footnoteDefinition` node.
 * @return {string} - Markdown footnote definition.
 */
function footnoteDefinition(node) {
  var id = node.identifier.toLowerCase();
  var content = this.all(node).join('\n\n' + repeat(' ', 4));

  return '[^' + id + ']: ' + content;
}
