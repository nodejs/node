// Flags: --experimental-stream-iter --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { brotliDecompressSync, gunzipSync, gzipSync } = require('zlib');
const { compressBrotli, compressGzip, decompressGzip } = require('zlib/iter');
const { bytes, from, pull } = require('stream/iter');

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

// An output buffer smaller than the batch high water mark fills repeatedly
// before a batch is ready, so the engine is re-entered from the write
// callback without yielding.
async function testSmallChunkSizeDecompression() {
  const input = Buffer.alloc(256 * 1024, 0x61);
  const output = await bytes(pull(from(gzipSync(input)),
                                  decompressGzip({ chunkSize: 1024 })));
  assert.deepStrictEqual(Buffer.from(output), input);
}

// Deterministic incompressible data. Brotli's buffering decisions depend on
// content, so random data would make the tests below flaky.
function incompressible(size) {
  const buffer = Buffer.allocUnsafe(size);
  let x = 0x9e3779b9;
  for (let i = 0; i < size; i++) {
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    buffer[i] = x & 0xff;
  }
  return buffer;
}

// Brotli buffers 1MB of incompressible input internally, so finalizing the
// stream produces more than one batch of output that must be yielded while
// the finish operation is still in progress.
async function testLargeFinishOutput() {
  const input = incompressible(1024 * 1024);
  const output = await bytes(pull(from(input), compressBrotli()));
  assert.deepStrictEqual(brotliDecompressSync(output), input);
}

// Unwrapping the transform function drops the validated fast path, so pull()
// delivers an explicit null flush signal to the compression transform.
async function testExplicitFlushSignal() {
  const input = incompressible(1024 * 1024);
  const { transform } = compressBrotli();
  const output = await bytes(pull(from(input), { transform }));
  assert.deepStrictEqual(brotliDecompressSync(output), input);
}

// Failures accessing or calling the source iterator's return() method must
// not replace the transform's successful completion.
async function testSourceReturnFailuresAreIgnored() {
  const input = Buffer.from('hello world');
  function makeSource(returnDescriptor) {
    let done = false;
    const iterator = {
      next() {
        if (done) return Promise.resolve({ done: true, value: undefined });
        done = true;
        return Promise.resolve({ done: false, value: [input] });
      },
    };
    Object.defineProperty(iterator, 'return', returnDescriptor);
    return { [Symbol.asyncIterator]() { return iterator; } };
  }

  const sources = [
    makeSource({ get: common.mustCall(() => { throw new Error('getter'); }) }),
    makeSource({
      value: common.mustCall(() => { throw new Error('return'); }),
    }),
  ];
  for (const source of sources) {
    const { signal } = new AbortController();
    const batches = [];
    for await (const batch of compressGzip().transform(source, { signal })) {
      batches.push(...batch);
    }
    assert.deepStrictEqual(gunzipSync(Buffer.concat(batches)), input);
  }
}

(async () => {
  // Run the memory measurement on its own so other tests don't skew it.
  await testDecompressionOutputIsBounded();
  await Promise.all([
    testSmallChunkSizeDecompression(),
    testLargeFinishOutput(),
    testExplicitFlushSignal(),
    testSourceReturnFailuresAreIgnored(),
  ]);
})().then(common.mustCall());
