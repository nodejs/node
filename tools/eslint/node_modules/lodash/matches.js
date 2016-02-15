var baseClone = require('./_baseClone'),
    baseMatches = require('./_baseMatches');

/**
 * Creates a function that performs a deep partial comparison between a given
 * object and `source`, returning `true` if the given object has equivalent
 * property values, else `false`.
 *
 * **Note:** This method supports comparing the same values as `_.isEqual`.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {Object} source The object of property values to match.
 * @returns {Function} Returns the new function.
 * @example
 *
 * var users = [
 *   { 'user': 'barney', 'age': 36, 'active': true },
 *   { 'user': 'fred',   'age': 40, 'active': false }
 * ];
 *
 * _.filter(users, _.matches({ 'age': 40, 'active': false }));
 * // => [{ 'user': 'fred', 'age': 40, 'active': false }]
 */
function matches(source) {
  return baseMatches(baseClone(source, true));
}

module.exports = matches;
