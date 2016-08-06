/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:util:get-indentation
 * @fileoverview Get indentation.
 */

'use strict';

/* Expose. */
module.exports = indentation;

/* A map of characters, and their column length,
 * which can be used as indentation. */
var characters = {' ': 1, '\t': 4};

/**
 * Gets indentation information for a line.
 *
 * @param {string} value - Indented line.
 * @return {Object} - Indetation information.
 */
function indentation(value) {
  var index = 0;
  var indent = 0;
  var character = value.charAt(index);
  var stops = {};
  var size;

  while (character in characters) {
    size = characters[character];

    indent += size;

    if (size > 1) {
      indent = Math.floor(indent / size) * size;
    }

    stops[indent] = index;

    character = value.charAt(++index);
  }

  return {indent: indent, stops: stops};
}
