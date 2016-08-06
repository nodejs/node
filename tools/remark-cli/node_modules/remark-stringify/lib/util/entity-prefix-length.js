/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:util:entity-prefix-length
 * @fileoverview Encode based on the identifier.
 */

'use strict';

/* Dependencies. */
var decode = require('parse-entities');

/* Expose. */
module.exports = length;

/**
 * Returns the length of HTML entity that is a prefix of
 * the given string (excluding the ampersand), 0 if it
 * does not start with an entity.
 *
 * @example
 *   entityPrefixLength('&copycat') // 4
 *   entityPrefixLength('&foo &amp &bar') // 0
 *
 * @param {string} value - Input string.
 * @return {number} - Length of an entity.
 */
function length(value) {
  var prefix;

  /* istanbul ignore if - Currently also tested for at
   * implemention, but we keep it here because that’s
   * proper. */
  if (value.charAt(0) !== '&') {
    return 0;
  }

  prefix = value.split('&', 2).join('&');

  return prefix.length - decode(prefix).length;
}
