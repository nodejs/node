/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:html
 * @fileoverview Stringify html.
 */

'use strict';

/* Expose. */
module.exports = html;

/**
 * Stringify html.
 *
 * @param {Object} node - `html` node.
 * @return {string} - html.
 */
function html(node) {
  return node.value;
}
