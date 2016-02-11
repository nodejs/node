'use strict';

/*
 * Constants.
 */

var WHITE_SPACE_COLLAPSABLE = /\s+/g;
var SPACE = ' ';

/**
 * Replace multiple white-space characters with a single space.
 *
 * @example
 *   collapse(' \t\nbar \nbaz\t'); // ' bar baz '
 *
 * @param {string} value - Value with uncollapsed white-space,
 *   coerced to string.
 * @return {string} - Value with collapsed white-space.
 */
function collapse(value) {
    return String(value).replace(WHITE_SPACE_COLLAPSABLE, SPACE);
}

/*
 * Expose.
 */

module.exports = collapse;
