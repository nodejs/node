/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:break
 * @fileoverview Stringify a break.
 */

'use strict';

/* Expose. */
module.exports = lineBreak;

/* Constants. */
var map = {true: '\\\n', false: '  \n'};

/**
 * Stringify a hard break.
 *
 * In Commonmark mode, trailing backslash form is used in order
 * to preserve trailing whitespace that the line may end with,
 * and also for better visibility.
 *
 * @return {string} - Markdown break.
 */
function lineBreak() {
  return map[this.options.commonmark];
}
