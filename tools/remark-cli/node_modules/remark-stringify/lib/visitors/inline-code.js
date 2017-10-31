/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:inline-code
 * @fileoverview Stringify inline code.
 */

'use strict';

/* Dependencies. */
var streak = require('longest-streak');
var repeat = require('repeat-string');

/* Expose. */
module.exports = inlineCode;

/**
 * Stringify inline code.
 *
 * Knows about internal ticks (`\``), and ensures one more
 * tick is used to enclose the inline code:
 *
 *     ```foo ``bar`` baz```
 *
 * Even knows about inital and final ticks:
 *
 *     `` `foo ``
 *     `` foo` ``
 *
 * @param {Object} node - `inlineCode` node.
 * @return {string} - Markdown inline code.
 */
function inlineCode(node) {
  var value = node.value;
  var ticks = repeat('`', streak(value, '`') + 1);
  var start = ticks;
  var end = ticks;

  if (value.charAt(0) === '`') {
    start += ' ';
  }

  if (value.charAt(value.length - 1) === '`') {
    end = ' ' + end;
  }

  return start + value + end;
}
