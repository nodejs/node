/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:link-reference
 * @fileoverview Stringify a link reference.
 */

'use strict';

/* Dependencies. */
var copy = require('../util/copy-identifier-encoding');
var label = require('../util/label');

/* Expose. */
module.exports = linkReference;

/**
 * Stringify a link reference.
 *
 * @param {Object} node - `linkReference` node.
 * @return {string} - Markdown link reference.
 */
function linkReference(node) {
  var self = this;
  var type = node.referenceType;
  var exit = self.enterLinkReference(self, node);
  var value = self.all(node).join('');

  exit();

  if (type === 'shortcut' || type === 'collapsed') {
    value = copy(value, node.identifier);
  }

  return '[' + value + ']' + label(node);
}
