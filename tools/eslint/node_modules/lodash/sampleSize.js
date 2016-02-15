var baseClamp = require('./_baseClamp'),
    baseRandom = require('./_baseRandom'),
    toArray = require('./toArray'),
    toInteger = require('./toInteger');

/**
 * Gets `n` random elements at unique keys from `collection` up to the
 * size of `collection`.
 *
 * @static
 * @memberOf _
 * @category Collection
 * @param {Array|Object} collection The collection to sample.
 * @param {number} [n=0] The number of elements to sample.
 * @returns {Array} Returns the random elements.
 * @example
 *
 * _.sampleSize([1, 2, 3], 2);
 * // => [3, 1]
 *
 * _.sampleSize([1, 2, 3], 4);
 * // => [2, 3, 1]
 */
function sampleSize(collection, n) {
  var index = -1,
      result = toArray(collection),
      length = result.length,
      lastIndex = length - 1;

  n = baseClamp(toInteger(n), 0, length);
  while (++index < n) {
    var rand = baseRandom(index, lastIndex),
        value = result[rand];

    result[rand] = result[index];
    result[index] = value;
  }
  result.length = n;
  return result;
}

module.exports = sampleSize;
