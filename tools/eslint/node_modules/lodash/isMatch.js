var baseIsMatch = require('./_baseIsMatch'),
    getMatchData = require('./_getMatchData');

/**
 * Performs a partial deep comparison between `object` and `source` to
 * determine if `object` contains equivalent property values. This method is
 * equivalent to a `_.matches` function when `source` is partially applied.
 *
 * **Note:** This method supports comparing the same values as `_.isEqual`.
 *
 * @static
 * @memberOf _
 * @since 3.0.0
 * @category Lang
 * @param {Object} object The object to inspect.
 * @param {Object} source The object of property values to match.
 * @returns {boolean} Returns `true` if `object` is a match, else `false`.
 * @example
 *
 * var object = { 'user': 'fred', 'age': 40 };
 *
 * _.isMatch(object, { 'age': 40 });
 * // => true
 *
 * _.isMatch(object, { 'age': 36 });
 * // => false
 */
function isMatch(object, source) {
  return object === source || baseIsMatch(object, source, getMatchData(source));
}

module.exports = isMatch;
