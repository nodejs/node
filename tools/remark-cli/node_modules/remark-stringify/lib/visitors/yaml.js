/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:yaml
 * @fileoverview Stringify yaml.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');

/* Expose. */
module.exports = yaml;

/**
 * Stringify `yaml`.
 *
 * @param {Object} node - `yaml` node.
 * @return {string} - Markdown yaml.
 */
function yaml(node) {
  var marker = repeat('-', 3);
  return marker + (node.value ? '\n' + node.value : '') + '\n' + marker;
}
