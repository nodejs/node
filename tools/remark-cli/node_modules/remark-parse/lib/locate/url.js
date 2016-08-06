/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:locate:url
 * @fileoverview Locate a URL.
 */

'use strict';

/* Expose. */
module.exports = locate;

var PROTOCOLS = ['https://', 'http://', 'mailto:'];

/**
 * Find a possible token.
 *
 * @param {string} value - Value to search.
 * @param {number} fromIndex - Index to start searching at.
 * @return {number} - Location.
 */
function locate(value, fromIndex) {
  var length = PROTOCOLS.length;
  var index = -1;
  var min = -1;
  var position;

  if (!this.options.gfm) {
    return -1;
  }

  while (++index < length) {
    position = value.indexOf(PROTOCOLS[index], fromIndex);

    if (position !== -1 && (position < min || min === -1)) {
      min = position;
    }
  }

  return min;
}
