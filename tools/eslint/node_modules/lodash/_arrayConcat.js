/**
 * Creates a new array concatenating `array` with `other`.
 *
 * @private
 * @param {Array} array The first array to concatenate.
 * @param {Array} other The second array to concatenate.
 * @returns {Array} Returns the new concatenated array.
 */
function arrayConcat(array, other) {
  var index = -1,
      length = array.length,
      othIndex = -1,
      othLength = other.length,
      result = Array(length + othLength);

  while (++index < length) {
    result[index] = array[index];
  }
  while (++othIndex < othLength) {
    result[index++] = other[othIndex];
  }
  return result;
}

module.exports = arrayConcat;
