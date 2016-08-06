/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:table-cell
 * @fileoverview Stringify a table-cell.
 */

'use strict';

/* Expose. */
module.exports = tableCell;

/**
 * Stringify a table cell.
 *
 * @param {Object} node - `tableCell` node.
 * @return {string} - Markdown table cell.
 */
function tableCell(node) {
  return this.all(node).join('');
}
