/**
 * @author Titus Wormer
 * @copyright 2014-2015 Titus Wormer
 * @license MIT
 * @module is-hidden
 * @fileoverview Check if `filename` is hidden (starts with a dot).
 */

'use strict';

/* eslint-env commonjs */

/**
 * Check if `filename` is hidden (starts with a dot).
 *
 * @param {string} filename - File-path to check.
 * @return {boolean} - Whether `filename` is hidden.
 * @throws {Error} - When `filename` is not a string.
 */
function isHidden(filename) {
    if (typeof filename !== 'string') {
        throw new Error('Expected string');
    }

    return filename.charAt(0) === '.';
}

module.exports = isHidden;
