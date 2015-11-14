var baseClone = require('./internal/baseClone'),
    baseMatchesProperty = require('./internal/baseMatchesProperty');

/**
 * Creates a function that performs a deep partial comparison between the
 * value at `path` of a given object to `srcValue`, returning `true` if the
 * object value is equivalent, else `false`.
 *
 * **Note:** This method supports comparing the same values as `_.isEqual`.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {Array|string} path The path of the property to get.
 * @param {*} srcValue The value to match.
 * @returns {Function} Returns the new function.
 * @example
 *
 * var users = [
 *   { 'user': 'barney' },
 *   { 'user': 'fred' }
 * ];
 *
 * _.find(users, _.matchesProperty('user', 'fred'));
 * // => { 'user': 'fred' }
 */
function matchesProperty(path, srcValue) {
  return baseMatchesProperty(path, baseClone(srcValue, true));
}

module.exports = matchesProperty;
