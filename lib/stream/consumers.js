'use strict';

const {
  JSONParse,
  SymbolAsyncIterator,
} = primordials;

const {
  TextDecoder,
} = require('internal/encoding');

const {
  Blob,
} = require('internal/blob');

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
  const iterator = stream[SymbolAsyncIterator]();
  let result = await iterator.next();
  while (!result.done) {
    chunks.push(result.value);
    result = await iterator.next();
  }
  return new Blob(chunks);
}

/**
 * @param {AsyncIterable|ReadableStream|Readable} stream
 * @returns {Promise<ArrayBuffer>}
 */
async function arrayBuffer(stream) {
  const ret = await blob(stream);
  return ret.arrayBuffer();
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
 * @returns {Promise<string>}
 */
async function text(stream) {
  const dec = new TextDecoder();
  let str = '';
  const iterator = stream[SymbolAsyncIterator]();
  let result = await iterator.next();
  while (!result.done) {
    const chunk = result.value;
    if (typeof chunk === 'string')
      str += chunk;
    else
      str += dec.decode(chunk, { stream: true });
    result = await iterator.next();
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
  text,
  json,
};
