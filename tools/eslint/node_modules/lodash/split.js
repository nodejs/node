var toString = require('./toString');

/**
 * Splits `string` by `separator`.
 *
 * **Note:** This method is based on [`String#split`](https://mdn.io/String/split).
 *
 * @static
 * @memberOf _
 * @category String
 * @param {string} [string=''] The string to split.
 * @param {RegExp|string} separator The separator pattern to split by.
 * @param {number} [limit] The length to truncate results to.
 * @returns {Array} Returns the new array of string segments.
 * @example
 *
 * _.split('a-b-c', '-', 2);
 * // => ['a', 'b']
 */
function split(string, separator, limit) {
  return toString(string).split(separator, limit);
}

module.exports = split;
