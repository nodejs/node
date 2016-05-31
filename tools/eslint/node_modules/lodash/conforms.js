var baseClone = require('./_baseClone'),
    baseConforms = require('./_baseConforms');

/**
 * Creates a function that invokes the predicate properties of `source` with
 * the corresponding property values of a given object, returning `true` if
 * all predicates return truthy, else `false`.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Util
 * @param {Object} source The object of property predicates to conform to.
 * @returns {Function} Returns the new spec function.
 * @example
 *
 * var users = [
 *   { 'user': 'barney', 'age': 36 },
 *   { 'user': 'fred',   'age': 40 }
 * ];
 *
 * _.filter(users, _.conforms({ 'age': _.partial(_.gt, _, 38) }));
 * // => [{ 'user': 'fred', 'age': 40 }]
 */
function conforms(source) {
  return baseConforms(baseClone(source, true));
}

module.exports = conforms;
