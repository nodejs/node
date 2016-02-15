var cloneArrayBuffer = require('./_cloneArrayBuffer');

/**
 * Creates a clone of `typedArray`.
 *
 * @private
 * @param {Object} typedArray The typed array to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Object} Returns the cloned typed array.
 */
function cloneTypedArray(typedArray, isDeep) {
  var buffer = typedArray.buffer,
      Ctor = typedArray.constructor;

  return new Ctor(isDeep ? cloneArrayBuffer(buffer) : buffer, typedArray.byteOffset, typedArray.length);
}

module.exports = cloneTypedArray;
