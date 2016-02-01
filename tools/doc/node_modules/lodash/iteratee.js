var baseIteratee = require('./internal/baseIteratee'),
    isArray = require('./isArray'),
    isObjectLike = require('./isObjectLike'),
    matches = require('./matches');

/**
 * Creates a function that invokes `func` with the arguments of the created
 * function. If `func` is a property name the created callback returns the
 * property value for a given element. If `func` is an object the created
 * callback returns `true` for elements that contain the equivalent object properties, otherwise it returns `false`.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {*} [func=_.identity] The value to convert to a callback.
 * @returns {Function} Returns the callback.
 * @example
 *
 * var users = [
 *   { 'user': 'barney', 'age': 36 },
 *   { 'user': 'fred',   'age': 40 }
 * ];
 *
 * // create custom iteratee shorthands
 * _.iteratee = _.wrap(_.iteratee, function(callback, func) {
 *   var p = /^(\S+)\s*([<>])\s*(\S+)$/.exec(func);
 *   return !p ? callback(func) : function(object) {
 *     return (p[2] == '>' ? object[p[1]] > p[3] : object[p[1]] < p[3]);
 *   };
 * });
 *
 * _.filter(users, 'age > 36');
 * // => [{ 'user': 'fred', 'age': 40 }]
 */
function iteratee(func) {
  return (isObjectLike(func) && !isArray(func))
    ? matches(func)
    : baseIteratee(func);
}

module.exports = iteratee;
