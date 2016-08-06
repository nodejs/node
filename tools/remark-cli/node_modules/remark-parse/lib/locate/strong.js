/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:locate:strong
 * @fileoverview Locate bold / strong / importance.
 */

'use strict';

/* Expose. */
module.exports = locate;

/**
 * Find a possible token.
 *
 * @param {string} value - Value to search.
 * @param {number} fromIndex - Index to start searching at.
 * @return {number} - Location.
 */
function locate(value, fromIndex) {
  var asterisk = value.indexOf('**', fromIndex);
  var underscore = value.indexOf('__', fromIndex);

  if (underscore === -1) {
    return asterisk;
  }

  if (asterisk === -1) {
    return underscore;
  }

  return underscore < asterisk ? underscore : asterisk;
}
