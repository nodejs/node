'use strict';

// Regression test for invalid memory access in the zlib bindings when the
// input/output ArrayBuffer of an in-flight asynchronous compression operation is
// detached (e.g. ArrayBuffer.prototype.transfer()) or resized
// (ResizableArrayBuffer.prototype.resize()) while the operation runs on the
// libuv threadpool.
//
// The worker thread reads/writes through raw pointers into JS-owned backing
// stores. Those pointers must remain valid for the whole operation even if the
// buffer is inspected, transferred or resized immediately after the work is
// scheduled. Before the fix, detaching or resizing the backing store while the
// worker ran caused invalid memory access and a deterministic crash.
//
// Each scenario uses a chunkSize large enough that the whole input is consumed
// in a single pass, so the operation (with the buffer safely pinned/copied)
// completes cleanly and produces the correct compressed output.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Deterministic, incompressible-ish byte pattern (period 256).
function makePattern(size) {
  const data = Buffer.allocUnsafe(size);
  for (let i = 0; i < size; i++) data[i] = (i * 31) & 0xff;
  return data;
}

// Compress `input` asynchronously, invalidating its backing store right after
// the work is scheduled, and return the compressed output. The operation must
// complete without corrupting native memory and must still produce the output
// that corresponds to the (original) input contents.
function compressAndInvalidate(name, factory, input, invalidate) {
  return new Promise((resolve, reject) => {
    const stream = factory();
    const chunks = [];

    const timer = setTimeout(() => {
      reject(new Error(`timed out: ${name}`));
    }, 60_000);

    stream.on('data', (chunk) => chunks.push(chunk));
    stream.on('error', (err) => {
      clearTimeout(timer);
      reject(err);
    });
    stream.on('end', () => {
      clearTimeout(timer);
      resolve(Buffer.concat(chunks));
    });

    stream.write(input);
    // Invalidate while the worker thread is (very likely) mid-operation.
    invalidate(input);
    stream.end();
  });
}

// For output-buffer invalidation the compressed data cannot be read back, so
// this only asserts that the stream reaches a terminal state (end/close/error)
// without corrupting native memory. Invalid memory access would crash instead.
// `setup` (optional) configures the stream before the write and returns the
// invalidator.
function runNoCrash(name, factory, input, setup) {
  return new Promise((resolve, reject) => {
    const stream = factory();
    stream.resume();

    const timer = setTimeout(() => {
      reject(new Error(`timed out: ${name}`));
    }, 60_000);

    stream.on('error', () => {
      clearTimeout(timer);
      resolve(name);
    });
    stream.on('end', () => {
      clearTimeout(timer);
      resolve(name);
    });
    stream.on('close', () => {
      clearTimeout(timer);
      resolve(name);
    });

    let invalidateOutput = () => {};
    if (setup) invalidateOutput = setup(stream);

    stream.write(input);
    try {
      invalidateOutput();
    } catch {
      // Invalidating the stream's own (possibly non-resizable) output buffer may
      // throw in the test; that is not a native crash.
      clearTimeout(timer);
      resolve(name);
      return;
    }
    stream.end();
  });
}

async function main() {
  // Sanity check: the common non-adversarial path still round-trips.
  const original = Buffer.from(
    'The quick brown fox jumps over the lazy dog.'.repeat(1000));
  for (const [compress, decompress] of [
    [zlib.deflate, zlib.inflateSync],
    [zlib.gzip, zlib.gunzipSync],
    [zlib.brotliCompress, zlib.brotliDecompressSync],
    [zlib.zstdCompress, zlib.zstdDecompressSync],
  ]) {
    const out = await new Promise((resolve, reject) => {
      compress(original, (err, b) => {
        if (err) reject(err);
        else resolve(b);
      });
    });
    assert.deepStrictEqual(decompress(out), original);
  }

  // 8MB input with a 32MB output buffer: the worker consumes the whole input in
  // one pass (the pattern is highly compressible), so there is no
  // re-processing of a possibly-invalidated buffer. The buffer is large enough
  // that the worker is still mid-operation when the invalidation runs.
  const size = 8 * 1024 * 1024;
  const chunkSize = 32 * 1024 * 1024;

  const scenarios = [
    {
      name: 'deflate input detach',
      factory: () => zlib.createDeflate({ chunkSize }),
      decompress: zlib.inflateSync,
      expected: () => makePattern(size),
      input: () => makePattern(size),
      invalidate: (input) => input.buffer.transfer(0),
    },
    {
      name: 'gzip input detach',
      factory: () => zlib.createGzip({ chunkSize }),
      decompress: zlib.gunzipSync,
      expected: () => makePattern(size),
      input: () => makePattern(size),
      invalidate: (input) => input.buffer.transfer(0),
    },
    {
      name: 'brotli input detach',
      factory: () => zlib.createBrotliCompress({ chunkSize }),
      decompress: zlib.brotliDecompressSync,
      expected: () => makePattern(size),
      input: () => makePattern(size),
      invalidate: (input) => input.buffer.transfer(0),
    },
    {
      name: 'zstd input detach',
      factory: () => zlib.createZstdCompress({ chunkSize }),
      decompress: zlib.zstdDecompressSync,
      expected: () => makePattern(size),
      input: () => makePattern(size),
      invalidate: (input) => input.buffer.transfer(0),
    },
    {
      // Resizing a ResizableArrayBuffer decommits pages, which is the most
      // reliable trigger: the engine must copy this input rather than hold a
      // raw pointer into JS-owned memory.
      name: 'deflate input resizable shrink',
      factory: () => zlib.createDeflate({ chunkSize }),
      decompress: zlib.inflateSync,
      expected: () => makePattern(size),
      input: () => {
        const rab = new ArrayBuffer(size, { maxByteLength: size });
        const data = Buffer.from(rab);
        for (let i = 0; i < size; i++) data[i] = (i * 31) & 0xff;
        return data;
      },
      invalidate: (input) => input.buffer.resize(4096),
    },
  ];

  for (const scenario of scenarios) {
    const compressed = await compressAndInvalidate(
      scenario.name, scenario.factory, scenario.input(), scenario.invalidate);
    assert.deepStrictEqual(
      scenario.decompress(compressed),
      scenario.expected(),
      `incorrect compressed output for ${scenario.name}`,
    );
  }

  // Invalidating the output buffer must not corrupt native memory either. The
  // default output buffer is non-resizable, but the user can replace it; the
  // engine must pin it (or stage writes to its own memory) while the worker
  // runs.
  const outputScenarios = [
    {
      name: 'deflate output detach',
      factory: () => zlib.createDeflate({ chunkSize }),
      setup: (stream) => {
        const outBuf = stream._outBuffer;
        return () => outBuf.buffer.transfer(0);
      },
    },
    {
      // Resizing a ResizableArrayBuffer decommits pages; the engine must stage
      // output in its own memory for resizable output buffers.
      name: 'deflate output resizable shrink',
      factory: () => zlib.createDeflate({ chunkSize }),
      setup: (stream) => {
        const rab = new ArrayBuffer(chunkSize, { maxByteLength: chunkSize });
        stream._outBuffer = Buffer.from(rab);
        return () => rab.resize(4096);
      },
    },
  ];

  for (const scenario of outputScenarios) {
    await runNoCrash(
      scenario.name, scenario.factory, makePattern(size), scenario.setup);
  }
}

main().then(common.mustCall());
