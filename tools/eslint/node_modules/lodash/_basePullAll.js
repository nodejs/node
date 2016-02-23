var basePullAllBy = require('./_basePullAllBy');

/**
 * The base implementation of `_.pullAll`.
 *
 * @private
 * @param {Array} array The array to modify.
 * @param {Array} values The values to remove.
 * @returns {Array} Returns `array`.
 */
function basePullAll(array, values) {
  return basePullAllBy(array, values);
}

module.exports = basePullAll;
