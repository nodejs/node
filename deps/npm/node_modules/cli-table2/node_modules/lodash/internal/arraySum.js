/**
 * A specialized version of `_.sum` for arrays without support for callback
 * shorthands and `this` binding..
 *
 * @private
 * @param {Array} array The array to iterate over.
 * @param {Function} iteratee The function invoked per iteration.
 * @returns {number} Returns the sum.
 */
function arraySum(array, iteratee) {
  var length = array.length,
      result = 0;

  while (length--) {
    result += +iteratee(array[length]) || 0;
  }
  return result;
}

module.exports = arraySum;
