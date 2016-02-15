var baseToPath = require('./_baseToPath'),
    isIndex = require('./_isIndex'),
    isKey = require('./_isKey'),
    last = require('./last'),
    parent = require('./_parent');

/** Used for built-in method references. */
var arrayProto = Array.prototype;

/** Built-in value references. */
var splice = arrayProto.splice;

/**
 * The base implementation of `_.pullAt` without support for individual
 * indexes or capturing the removed elements.
 *
 * @private
 * @param {Array} array The array to modify.
 * @param {number[]} indexes The indexes of elements to remove.
 * @returns {Array} Returns `array`.
 */
function basePullAt(array, indexes) {
  var length = array ? indexes.length : 0,
      lastIndex = length - 1;

  while (length--) {
    var index = indexes[length];
    if (lastIndex == length || index != previous) {
      var previous = index;
      if (isIndex(index)) {
        splice.call(array, index, 1);
      }
      else if (!isKey(index, array)) {
        var path = baseToPath(index),
            object = parent(array, path);

        if (object != null) {
          delete object[last(path)];
        }
      }
      else {
        delete array[index];
      }
    }
  }
  return array;
}

module.exports = basePullAt;
