var arrayMap = require('./_arrayMap'),
    baseIndexOf = require('./_baseIndexOf');

/** Used for built-in method references. */
var arrayProto = Array.prototype;

/** Built-in value references. */
var splice = arrayProto.splice;

/**
 * The base implementation of `_.pullAllBy` without support for iteratee
 * shorthands.
 *
 * @private
 * @param {Array} array The array to modify.
 * @param {Array} values The values to remove.
 * @param {Function} [iteratee] The iteratee invoked per element.
 * @returns {Array} Returns `array`.
 */
function basePullAllBy(array, values, iteratee) {
  var index = -1,
      length = values.length,
      seen = array;

  if (iteratee) {
    seen = arrayMap(array, function(value) { return iteratee(value); });
  }
  while (++index < length) {
    var fromIndex = 0,
        value = values[index],
        computed = iteratee ? iteratee(value) : value;

    while ((fromIndex = baseIndexOf(seen, computed, fromIndex)) > -1) {
      if (seen !== array) {
        splice.call(seen, fromIndex, 1);
      }
      splice.call(array, fromIndex, 1);
    }
  }
  return array;
}

module.exports = basePullAllBy;
