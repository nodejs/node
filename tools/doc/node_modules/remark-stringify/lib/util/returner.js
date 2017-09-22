/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:util:returner
 * @fileoverview Return the given value.
 */

'use strict';

/* Expose. */
module.exports = returner;

/**
 * @param {*} value - Anything.
 * @return {*} - Given `value`.
 */
function returner(value) {
  return value;
}
