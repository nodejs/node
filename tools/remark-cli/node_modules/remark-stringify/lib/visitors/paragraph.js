/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:paragraph
 * @fileoverview Stringify a paragraph.
 */

'use strict';

/* Expose. */
module.exports = paragraph;

/**
 * Stringify a paragraph.
 *
 * @param {Object} node - `paragraph` node.
 * @return {string} - Markdown paragraph.
 */
function paragraph(node) {
  return this.all(node).join('');
}
