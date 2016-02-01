var baseFind = require('./internal/baseFind'),
    baseForOwnRight = require('./internal/baseForOwnRight'),
    baseIteratee = require('./internal/baseIteratee');

/**
 * This method is like `_.findKey` except that it iterates over elements of
 * a collection in the opposite order.
 *
 * @static
 * @memberOf _
 * @category Object
 * @param {Object} object The object to search.
 * @param {Function|Object|string} [predicate=_.identity] The function invoked per iteration.
 * @returns {string|undefined} Returns the key of the matched element, else `undefined`.
 * @example
 *
 * var users = {
 *   'barney':  { 'age': 36, 'active': true },
 *   'fred':    { 'age': 40, 'active': false },
 *   'pebbles': { 'age': 1,  'active': true }
 * };
 *
 * _.findLastKey(users, function(o) { return o.age < 40; });
 * // => returns 'pebbles' assuming `_.findKey` returns 'barney'
 *
 * // using the `_.matches` iteratee shorthand
 * _.findLastKey(users, { 'age': 36, 'active': true });
 * // => 'barney'
 *
 * // using the `_.matchesProperty` iteratee shorthand
 * _.findLastKey(users, ['active', false]);
 * // => 'fred'
 *
 * // using the `_.property` iteratee shorthand
 * _.findLastKey(users, 'active');
 * // => 'pebbles'
 */
function findLastKey(object, predicate) {
  return baseFind(object, baseIteratee(predicate, 3), baseForOwnRight, true);
}

module.exports = findLastKey;
