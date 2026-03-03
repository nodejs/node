'use strict';

// New Streams API - Utility Functions

const {
  ArrayPrototypeSlice,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const { TextEncoder } = require('internal/encoding');

// Shared TextEncoder instance for string conversion.
const encoder = new TextEncoder();

/**
 * Convert a chunk (string or Uint8Array) to Uint8Array.
 * Strings are UTF-8 encoded.
 * @param {Uint8Array|string} chunk
 * @returns {Uint8Array}
 */
function toUint8Array(chunk) {
  if (typeof chunk === 'string') {
    return encoder.encode(chunk);
  }
  return chunk;
}

/**
 * Check if all chunks in an array are already Uint8Array (no strings).
 * Short-circuits on the first string found.
 * @param {Array<Uint8Array|string>} chunks
 * @returns {boolean}
 */
function allUint8Array(chunks) {
  for (let i = 0; i < chunks.length; i++) {
    if (typeof chunks[i] === 'string') return false;
  }
  return true;
}

/**
 * Calculate total byte length of an array of chunks.
 * @param {Uint8Array[]} chunks
 * @returns {number}
 */
function totalByteLength(chunks) {
  let total = 0;
  for (let i = 0; i < chunks.length; i++) {
    total += chunks[i].byteLength;
  }
  return total;
}

/**
 * Concatenate multiple Uint8Arrays into a single Uint8Array.
 * @param {Uint8Array[]} chunks
 * @returns {Uint8Array}
 */
function concatBytes(chunks) {
  if (chunks.length === 0) {
    return new Uint8Array(0);
  }
  if (chunks.length === 1) {
    return chunks[0];
  }

  const total = totalByteLength(chunks);
  const result = new Uint8Array(total);
  let offset = 0;
  for (let i = 0; i < chunks.length; i++) {
    TypedArrayPrototypeSet(result, chunks[i], offset);
    offset += chunks[i].byteLength;
  }
  return result;
}

/**
 * Check if a value is PullOptions (object without transform or write property).
 * @param {unknown} value
 * @returns {boolean}
 */
function isPullOptions(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !('transform' in value) &&
    !('write' in value)
  );
}

/**
 * Parse variadic arguments for pull/pullSync.
 * Returns { transforms, options }
 * @param {Array} args
 * @returns {{ transforms: Array, options: object|undefined }}
 */
function parsePullArgs(args) {
  if (args.length === 0) {
    return { transforms: [], options: undefined };
  }

  const last = args[args.length - 1];
  if (isPullOptions(last)) {
    return {
      transforms: ArrayPrototypeSlice(args, 0, -1),
      options: last,
    };
  }

  return { transforms: args, options: undefined };
}

module.exports = {
  toUint8Array,
  allUint8Array,
  concatBytes,
  isPullOptions,
  parsePullArgs,
};
