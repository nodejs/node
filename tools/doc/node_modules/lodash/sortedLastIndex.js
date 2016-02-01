var baseSortedIndex = require('./internal/baseSortedIndex');

/**
 * This method is like `_.sortedIndex` except that it returns the highest
 * index at which `value` should be inserted into `array` in order to
 * maintain its sort order.
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {Array} array The sorted array to inspect.
 * @param {*} value The value to evaluate.
 * @returns {number} Returns the index at which `value` should be inserted into `array`.
 * @example
 *
 * _.sortedLastIndex([4, 5], 4);
 * // => 1
 */
function sortedLastIndex(array, value) {
  return baseSortedIndex(array, value, true);
}

module.exports = sortedLastIndex;
