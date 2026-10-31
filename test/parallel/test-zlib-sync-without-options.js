'use strict';
// Flags: --enable-async-hooks
// Synchronous convenience methods called without options skip the stream and
// reuse an internal output buffer. Check that they behave exactly like calls
// that pass an empty options object, which go through the stream.

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const fs = require('fs');
const zlib = require('zlib');

const { Z_DEFAULT_CHUNK } = zlib.constants;

const pairs = [
  ['deflateSync', 'inflateSync'],
  ['gzipSync', 'gunzipSync'],
  ['gzipSync', 'unzipSync'],
  ['deflateRawSync', 'inflateRawSync'],
  ['brotliCompressSync', 'brotliDecompressSync'],
  ['zstdCompressSync', 'zstdDecompressSync'],
];

const random = Buffer.alloc(3 * Z_DEFAULT_CHUNK);
{
  let seed = 0x9e3779b9;
  for (let i = 0; i < random.length; i++) {
    seed ^= seed << 13;
    seed ^= seed >>> 17;
    seed ^= seed << 5;
    random[i] = seed & 0xff;
  }
}
const text = fs.readFileSync(__filename);

// Outputs that fit in one output buffer, fill it exactly, and span several.
const inputs = [
  Buffer.alloc(0),
  Buffer.from('hello'),
  text,
  Buffer.alloc(Z_DEFAULT_CHUNK, 'a'),
  Buffer.alloc(Z_DEFAULT_CHUNK + 1, 'b'),
  Buffer.concat([text, random, text]),
];

for (const [compress, decompress] of pairs) {
  for (const input of inputs) {
    const label = `${compress}/${decompress} ${input.length}`;
    const compressed = zlib[compress](input);
    assert.ok(Buffer.isBuffer(compressed), label);
    assert.deepStrictEqual(compressed, zlib[compress](input, {}), label);

    const decompressed = zlib[decompress](compressed);
    assert.ok(Buffer.isBuffer(decompressed), label);
    assert.deepStrictEqual(decompressed, input, label);
    assert.deepStrictEqual(decompressed, zlib[decompress](compressed, {}),
                           label);
  }

  // Every input type.
  const expected = zlib[decompress](zlib[compress]('blah'.repeat(8)));
  for (const view of [
    'blah'.repeat(8),
    ...common.getBufferSources(Buffer.from('blah'.repeat(8))),
  ]) {
    assert.deepStrictEqual(zlib[decompress](zlib[compress](view)), expected);
  }
}

// Results never share memory with each other.
{
  const a = zlib.deflateSync('first');
  const b = zlib.deflateSync('second');
  const first = zlib.inflateSync(a);
  const second = zlib.inflateSync(b);
  assert.strictEqual(first.toString(), 'first');
  assert.strictEqual(second.toString(), 'second');
  second.fill(0);
  assert.strictEqual(first.toString(), 'first');
  assert.strictEqual(zlib.inflateSync(a).toString(), 'first');
}

// Errors are the same as through the stream.
for (const [method, input] of [
  ['inflateSync', Buffer.from('not compressed')],
  ['gunzipSync', Buffer.from('not compressed')],
  ['unzipSync', Buffer.from('not compressed')],
  ['inflateRawSync', Buffer.from('not compressed')],
  ['brotliDecompressSync', Buffer.from('not compressed')],
  ['zstdDecompressSync', Buffer.from('not compressed')],
  ['inflateSync', zlib.deflateSync(text).subarray(0, 100)],
  ['gunzipSync', zlib.gzipSync(text).subarray(0, 100)],
  ['brotliDecompressSync', zlib.brotliCompressSync(text).subarray(0, 100)],
  ['zstdDecompressSync', zlib.zstdCompressSync(text).subarray(0, 100)],
]) {
  let expected;
  assert.throws(() => zlib[method](input, {}), (err) => {
    expected = err;
    return true;
  });
  assert.throws(() => zlib[method](input), (err) => {
    assert.strictEqual(err.constructor, expected.constructor);
    assert.strictEqual(err.message, expected.message);
    assert.strictEqual(err.code, expected.code);
    assert.strictEqual(err.errno, expected.errno);
    return true;
  }, method);
}

for (const [compress] of pairs) {
  assert.throws(() => zlib[compress](42), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}

// A synchronous call made from an async hook while another one is running.
{
  const compressed = zlib.deflateSync(text);
  let nested;
  const hook = async_hooks.createHook({
    before() {
      if (nested === undefined) {
        nested = null;
        nested = zlib.inflateSync(compressed);
      }
    },
  }).enable();
  // The error is reported through a callback into JavaScript, which runs the
  // before hook.
  assert.throws(() => zlib.inflateSync(Buffer.from('not compressed')), {
    code: 'Z_DATA_ERROR',
  });
  hook.disable();
  assert.deepStrictEqual(nested, text);
  assert.deepStrictEqual(zlib.inflateSync(compressed), text);
}
