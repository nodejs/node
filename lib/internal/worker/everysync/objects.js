'use strict';

const {
  DataView,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_BUFFER_SIZE,
  },
} = require('internal/errors');

const { serialize, deserialize } = require('v8');

/**
 * Reads an object from a shared memory buffer
 * @param {SharedArrayBuffer} buffer - The shared memory buffer containing serialized data
 * @param {number} [byteOffset=0] - Byte offset where the data begins
 * @returns {any} - The deserialized object
 */
function read(buffer, byteOffset = 0) {
  const view = new DataView(buffer, byteOffset);
  const length = view.getUint32(0, true);
  const object = deserialize(new Uint8Array(buffer, byteOffset + 4, length));
  return object;
}

/**
 * Writes an object to a shared memory buffer
 * @param {SharedArrayBuffer} buffer - The shared memory buffer to write to
 * @param {any} object - The object to serialize and write
 * @param {number} [byteOffset=0] - Byte offset where to write the data
 * @throws {Error} If the buffer is too small and not growable
 */
function write(buffer, object, byteOffset = 0) {
  const data = serialize(object);

  if (buffer.byteLength < data.byteLength + 4 + byteOffset) {
    // Check if buffer is growable (has grow method from ShareArrayBuffer.prototype)
    if (typeof buffer.grow !== 'function') {
      throw new ERR_INVALID_BUFFER_SIZE('Buffer is too small and not growable');
    }
    buffer.grow(data.byteLength + 4 + byteOffset);
  }

  const view = new DataView(buffer, byteOffset);
  view.setUint32(0, data.byteLength, true);
  new Uint8Array(buffer, byteOffset + 4).set(data);
}

module.exports = {
  read,
  write,
};
