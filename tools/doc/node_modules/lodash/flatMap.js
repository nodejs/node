var arrayMap = require('./internal/arrayMap'),
    baseFlatten = require('./internal/baseFlatten'),
    baseIteratee = require('./internal/baseIteratee');

/**
 * Creates an array of flattened values by running each element in `array`
 * through `iteratee` and concating its result to the other mapped values.
 * The iteratee is invoked with three arguments: (value, index|key, array).
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {Array} array The array to iterate over.
 * @param {Function|Object|string} [iteratee=_.identity] The function invoked per iteration.
 * @returns {Array} Returns the new array.
 * @example
 *
 * function duplicate(n) {
 *   return [n, n];
 * }
 *
 * _.flatMap([1, 2], duplicate);
 * // => [1, 1, 2, 2]
 */
function flatMap(array, iteratee) {
  var length = array ? array.length : 0;
  return length ? baseFlatten(arrayMap(array, baseIteratee(iteratee, 3))) : [];
}

module.exports = flatMap;
