var eq = require('./eq');

/**
 * The base implementation of `_.sortedUniqBy` without support for iteratee
 * shorthands.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {Function} [iteratee] The iteratee invoked per element.
 * @returns {Array} Returns the new duplicate free array.
 */
function baseSortedUniqBy(array, iteratee) {
  var index = 0,
      length = array.length,
      value = array[0],
      computed = iteratee ? iteratee(value) : value,
      seen = computed,
      resIndex = 1,
      result = [value];

  while (++index < length) {
    value = array[index],
    computed = iteratee ? iteratee(value) : value;

    if (!eq(computed, seen)) {
      seen = computed;
      result[resIndex++] = value;
    }
  }
  return result;
}

module.exports = baseSortedUniqBy;
