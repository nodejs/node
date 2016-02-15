/**
 * Creates a clone of  `buffer`.
 *
 * @private
 * @param {Buffer} buffer The buffer to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Buffer} Returns the cloned buffer.
 */
function cloneBuffer(buffer, isDeep) {
  if (isDeep) {
    return buffer.slice();
  }
  var Ctor = buffer.constructor,
      result = new Ctor(buffer.length);

  buffer.copy(result);
  return result;
}

module.exports = cloneBuffer;
