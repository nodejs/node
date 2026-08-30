// Flags: --experimental-stream-iter --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { gzipSync } = require('zlib');
const { decompressGzip } = require('zlib/iter');
const { from } = require('stream/iter');

async function testDecompressionOutputIsBounded() {
  let input = Buffer.alloc(32 * 1024 * 1024, 0x61);
  const compressed = gzipSync(input);
  input = null;
  for (let i = 0; i < 3; i++) globalThis.gc();

  const before = process.memoryUsage().arrayBuffers;
  const controller = new AbortController();
  const iterator = decompressGzip().transform(from(compressed), {
    signal: controller.signal,
  })[Symbol.asyncIterator]();
  const first = await iterator.next();
  for (let i = 0; i < 3; i++) globalThis.gc();
  const retained = process.memoryUsage().arrayBuffers - before;

  assert.strictEqual(first.done, false);
  assert.ok(retained < 8 * 1024 * 1024,
            `decompression retained ${retained} bytes before backpressure`);
  await iterator.return();
}

testDecompressionOutputIsBounded().then(common.mustCall());
