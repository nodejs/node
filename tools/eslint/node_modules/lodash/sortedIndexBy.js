var baseIteratee = require('./_baseIteratee'),
    baseSortedIndexBy = require('./_baseSortedIndexBy');

/**
 * This method is like `_.sortedIndex` except that it accepts `iteratee`
 * which is invoked for `value` and each element of `array` to compute their
 * sort ranking. The iteratee is invoked with one argument: (value).
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {Array} array The sorted array to inspect.
 * @param {*} value The value to evaluate.
 * @param {Function|Object|string} [iteratee=_.identity] The iteratee invoked per element.
 * @returns {number} Returns the index at which `value` should be inserted into `array`.
 * @example
 *
 * var dict = { 'thirty': 30, 'forty': 40, 'fifty': 50 };
 *
 * _.sortedIndexBy(['thirty', 'fifty'], 'forty', _.propertyOf(dict));
 * // => 1
 *
 * // The `_.property` iteratee shorthand.
 * _.sortedIndexBy([{ 'x': 4 }, { 'x': 5 }], { 'x': 4 }, 'x');
 * // => 0
 */
function sortedIndexBy(array, value, iteratee) {
  return baseSortedIndexBy(array, value, baseIteratee(iteratee));
}

module.exports = sortedIndexBy;
