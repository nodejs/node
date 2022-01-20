/*! typedarray-to-buffer. MIT License. Feross Aboukhadijeh <https://feross.org/opensource> */
/**
 * Convert a typed array to a Buffer without a copy
 *
 * Author:   Feross Aboukhadijeh <https://feross.org>
 * License:  MIT
 *
 * `npm install typedarray-to-buffer`
 */

module.exports = function typedarrayToBuffer (arr) {
  return ArrayBuffer.isView(arr)
    // To avoid a copy, use the typed array's underlying ArrayBuffer to back
    // new Buffer, respecting the "view", i.e. byteOffset and byteLength
    ? Buffer.from(arr.buffer, arr.byteOffset, arr.byteLength)
    // Pass through all other types to `Buffer.from`
    : Buffer.from(arr)
}
