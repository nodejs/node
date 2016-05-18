var baseToPairs = require('./_baseToPairs'),
    keysIn = require('./keysIn');

/**
 * Creates an array of own and inherited enumerable string keyed-value pairs
 * for `object` which can be consumed by `_.fromPairs`.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @alias entriesIn
 * @category Object
 * @param {Object} object The object to query.
 * @returns {Array} Returns the new array of key-value pairs.
 * @example
 *
 * function Foo() {
 *   this.a = 1;
 *   this.b = 2;
 * }
 *
 * Foo.prototype.c = 3;
 *
 * _.toPairsIn(new Foo);
 * // => [['a', 1], ['b', 2], ['c', 1]] (iteration order is not guaranteed)
 */
function toPairsIn(object) {
  return baseToPairs(object, keysIn(object));
}

module.exports = toPairsIn;
