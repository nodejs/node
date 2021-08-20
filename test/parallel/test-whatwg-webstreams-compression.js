// Flags: --no-warnings
'use strict';

const common = require('../common');

const {
  CompressionStream,
  DecompressionStream,
} = require('stream/web');

const assert = require('assert');
const dec = new TextDecoder();

async function test(format) {
  const gzip = new CompressionStream(format);
  const gunzip = new DecompressionStream(format);

  gzip.readable.pipeTo(gunzip.writable).then(common.mustCall());

  const reader = gunzip.readable.getReader();
  const writer = gzip.writable.getWriter();

  await Promise.all([
    reader.read().then(({ value, done }) => {
      assert.strictEqual(dec.decode(value), 'hello');
    }),
    reader.read().then(({ done }) => assert(done)),
    writer.write('hello'),
    writer.close(),
  ]);
}

Promise.all(['gzip', 'deflate'].map((i) => test(i))).then(common.mustCall());

[1, 'hello', false, {}].forEach((i) => {
  assert.throws(() => new CompressionStream(i), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => new DecompressionStream(i), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
});

assert.throws(
  () => Reflect.get(CompressionStream.prototype, 'readable', {}), {
    code: 'ERR_INVALID_THIS',
  });
assert.throws(
  () => Reflect.get(CompressionStream.prototype, 'writable', {}), {
    code: 'ERR_INVALID_THIS',
  });
assert.throws(
  () => Reflect.get(DecompressionStream.prototype, 'readable', {}), {
    code: 'ERR_INVALID_THIS',
  });
assert.throws(
  () => Reflect.get(DecompressionStream.prototype, 'writable', {}), {
    code: 'ERR_INVALID_THIS',
  });
