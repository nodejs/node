var baseSortedUniqBy = require('./baseSortedUniqBy');

/**
 * The base implementation of `_.sortedUniq`.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @returns {Array} Returns the new duplicate free array.
 */
function baseSortedUniq(array) {
  return baseSortedUniqBy(array);
}

module.exports = baseSortedUniq;
