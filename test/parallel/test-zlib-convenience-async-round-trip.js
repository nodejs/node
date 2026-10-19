'use strict';
// The asynchronous convenience methods know the whole input up front, so they
// process it with the finish flush in a single write. Check that a call whose
// output fits in one output buffer takes exactly one threadpool round trip.

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const zlib = require('zlib');

const input = Buffer.from('hello world '.repeat(64));
// [method, argument, function that turns the result back into `input`]
const cases = [
  ['deflate', input, zlib.inflateSync],
  ['inflate', zlib.deflateSync(input)],
  ['gzip', input, zlib.gunzipSync],
  ['gunzip', zlib.gzipSync(input)],
  ['unzip', zlib.gzipSync(input)],
  ['deflateRaw', input, zlib.inflateRawSync],
  ['inflateRaw', zlib.deflateRawSync(input)],
  ['brotliCompress', input, zlib.brotliDecompressSync],
  ['brotliDecompress', zlib.brotliCompressSync(input)],
  ['zstdDecompress', zlib.zstdCompressSync(input)],
];

// Counts the callbacks into JavaScript (`before` hooks) of each ZLIB resource
// created while `fn` runs.
async function countRoundTrips(fn) {
  const callbacks = new Map();
  const hook = async_hooks.createHook({
    init(asyncId, type) {
      if (type === 'ZLIB') callbacks.set(asyncId, 0);
    },
    before(asyncId) {
      if (callbacks.has(asyncId))
        callbacks.set(asyncId, callbacks.get(asyncId) + 1);
    },
  }).enable();
  try {
    await fn();
  } finally {
    hook.disable();
  }
  return [...callbacks.values()];
}

function callAsync(method, buffer, options = {}) {
  return new Promise((resolve, reject) => {
    zlib[method](buffer, options, common.mustCall((err, result) => {
      if (err) reject(err);
      else resolve(result);
    }));
  });
}

(async () => {
  for (const [method, buffer, undo = (result) => result] of cases) {
    let result;
    const roundTrips = await countRoundTrips(async () => {
      result = await callAsync(method, buffer);
    });
    assert.deepStrictEqual(roundTrips, [1], method);
    assert.deepStrictEqual(undo(result), input, method);
  }

  // Errors are also reported after a single round trip.
  let error;
  const roundTrips = await countRoundTrips(async () => {
    error = await callAsync('inflate', Buffer.from('not zlib data'))
      .catch((err) => err);
  });
  assert.deepStrictEqual(roundTrips, [1]);
  assert.strictEqual(error.code, 'Z_DATA_ERROR');
})().then(common.mustCall());
