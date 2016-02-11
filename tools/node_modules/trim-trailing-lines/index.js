'use strict';

/*
 * Constants.
 */

var LINE = '\n';

/**
 * Remove final newline characters from `value`.
 *
 * @example
 *   trimTrailingLines('foo\nbar'); // 'foo\nbar'
 *   trimTrailingLines('foo\nbar\n'); // 'foo\nbar'
 *   trimTrailingLines('foo\nbar\n\n'); // 'foo\nbar'
 *
 * @param {string} value - Value with trailing newlines,
 *   coerced to string.
 * @return {string} - Value without trailing newlines.
 */
function trimTrailingLines(value) {
    var index;

    value = String(value);
    index = value.length;

    while (value.charAt(--index) === LINE) { /* empty */ }

    return value.slice(0, index + 1);
}

/*
 * Expose.
 */

module.exports = trimTrailingLines;
