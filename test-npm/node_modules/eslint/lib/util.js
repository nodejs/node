/**
 * @fileoverview Common utilities.
 */
"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

/**
 * Gets the last element of a given array.
 * @param {any[]} xs - An array to get.
 * @returns {any|null} The last element, or `null` if the array is empty.
 */
function getLast(xs) {
    var length = xs.length;
    return (length === 0 ? null : xs[length - 1]);
}

module.exports = {
    getLast: getLast
};
