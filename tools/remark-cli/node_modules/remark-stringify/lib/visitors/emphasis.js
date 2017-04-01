/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:emphasis
 * @fileoverview Stringify a emphasis.
 */

'use strict';

/* Expose. */
module.exports = emphasis;

/**
 * Stringify a `emphasis`.
 *
 * The marker used is configurable through `emphasis`, which
 * defaults to an underscore (`'_'`) but also accepts an
 * asterisk (`'*'`):
 *
 *     *foo*
 *
 * @param {Object} node - `emphasis` node.
 * @return {string} - Markdown emphasis.
 */
function emphasis(node) {
  var marker = this.options.emphasis;
  return marker + this.all(node).join('') + marker;
}
