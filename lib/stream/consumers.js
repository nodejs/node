'use strict';

const {
  ArrayBuffer,
  ArrayBufferPrototypeGetResizable,
  JSONParse,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const {
  createUnsafeArrayBuffer,
  kMaxLength,
} = internalBinding('buffer');

const assert = require('internal/assert');

const {
  ERR_BUFFER_TOO_LARGE,
} = require('internal/errors').codes;

const {
  TextDecoder,
} = require('internal/encoding');

const {
  Blob,
} = require('internal/blob');

const {
  isArrayBuffer,
  isUint8Array,
} = require('internal/util/types');

const {
  Buffer,
} = require('buffer');

/**
 * @typedef {import('../internal/webstreams/readablestream').ReadableStream
 * } ReadableStream
 * @typedef {import('../internal/streams/readable')} Readable
 */

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<Blob>}
 */
async function blob(stream) {
  const chunks = [];
  for await (const chunk of stream)
    chunks.push(chunk);
  return new Blob(chunks);
}

/**
 * @param {any[]} chunks
 * @returns {ArrayBuffer | null}
 */
function concatUint8Arrays(chunks) {
  let totalLength = 0;
  for (let i = 0; i < chunks.length; i++) {
    const chunk = chunks[i];
    if (!isUint8Array(chunk))
      return null;
    const backing = TypedArrayPrototypeGetBuffer(chunk);
    if (!isArrayBuffer(backing) || ArrayBufferPrototypeGetResizable(backing))
      return null;
    totalLength += TypedArrayPrototypeGetByteLength(chunk);
  }

  if (totalLength === 0)
    return new ArrayBuffer(0);
  if (totalLength > kMaxLength)
    throw new ERR_BUFFER_TOO_LARGE(kMaxLength);

  const result = createUnsafeArrayBuffer(totalLength);
  const view = new Uint8Array(result);
  let offset = 0;
  for (let i = 0; i < chunks.length; i++) {
    const chunk = chunks[i];
    const byteLength = TypedArrayPrototypeGetByteLength(chunk);
    if (byteLength !== 0) {
      TypedArrayPrototypeSet(view, chunk, offset);
      offset += byteLength;
    }
  }
  assert(offset === totalLength);
  return result;
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<ArrayBuffer>}
 */
async function arrayBuffer(stream) {
  const chunks = [];
  for await (const chunk of stream)
    chunks[chunks.length] = chunk;
  return concatUint8Arrays(chunks) ?? new Blob(chunks).arrayBuffer();
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<Buffer>}
 */
async function buffer(stream) {
  return Buffer.from(await arrayBuffer(stream));
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<Uint8Array>}
 */
async function bytes(stream) {
  return new Uint8Array(await arrayBuffer(stream));
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<string>}
 */
async function text(stream) {
  const dec = new TextDecoder();
  let str = '';
  for await (const chunk of stream) {
    if (typeof chunk === 'string')
      str += chunk;
    else
      str += dec.decode(chunk, { stream: true });
  }
  // Flush the streaming TextDecoder so that any pending
  // incomplete multibyte characters are handled.
  str += dec.decode(undefined, { stream: false });
  return str;
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<any>}
 */
async function json(stream) {
  const str = await text(stream);
  return JSONParse(str);
}

module.exports = {
  arrayBuffer,
  blob,
  buffer,
  bytes,
  text,
  json,
  concat: require('stream/consumers/concat'),
};
