var Uint8Array = require('./Uint8Array');

/**
 * Creates a clone of `buffer`.
 *
 * @private
 * @param {ArrayBuffer} buffer The array buffer to clone.
 * @returns {ArrayBuffer} Returns the cloned array buffer.
 */
function cloneBuffer(buffer) {
  var Ctor = buffer.constructor,
      result = new Ctor(buffer.byteLength),
      view = new Uint8Array(result);

  view.set(new Uint8Array(buffer));
  return result;
}

module.exports = cloneBuffer;
