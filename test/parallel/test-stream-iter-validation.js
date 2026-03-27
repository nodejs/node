// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from, fromSync, pull, pullSync, pipeTo,
  push, duplex, broadcast, Broadcast, share, shareSync,
  Share, SyncShare,
  bytes, bytesSync, text, textSync,
  arrayBuffer, arrayBufferSync, array, arraySync,
  tap, tapSync,
} = require('stream/iter');
const {
  compressGzip, compressBrotli, compressZstd,
  decompressGzip, decompressBrotli, decompressZstd,
} = require('zlib/iter');

// =============================================================================
// push() validation
// =============================================================================

// HighWaterMark must be integer >= 1
assert.throws(() => push({ highWaterMark: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => push({ highWaterMark: 1.5 }), { code: 'ERR_OUT_OF_RANGE' });
// Values < 1 are clamped to 1
assert.strictEqual(push({ highWaterMark: 0 }).writer.desiredSize, 1);
assert.strictEqual(push({ highWaterMark: -1 }).writer.desiredSize, 1);
assert.strictEqual(push({ highWaterMark: -100 }).writer.desiredSize, 1);
// MAX_SAFE_INTEGER is accepted
assert.strictEqual(push({ highWaterMark: Number.MAX_SAFE_INTEGER }).writer.desiredSize,
                   Number.MAX_SAFE_INTEGER);
// Values above MAX_SAFE_INTEGER are rejected by validateInteger
assert.throws(() => push({ highWaterMark: Number.MAX_SAFE_INTEGER + 1 }),
              { code: 'ERR_OUT_OF_RANGE' });

// Signal must be AbortSignal
assert.throws(() => push({ signal: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => push({ signal: {} }), { code: 'ERR_INVALID_ARG_TYPE' });

// Transforms must be functions or transform objects
assert.throws(() => push(42, {}), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => push('bad', {}), { code: 'ERR_INVALID_ARG_TYPE' });

// Writer.writev requires array
{
  const { writer } = push();
  assert.throws(() => writer.writev('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => writer.writev(42), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => writer.writevSync('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
  writer.endSync();
}

// Writer.write rejects non-string/non-Uint8Array
{
  const { writer } = push();
  assert.throws(() => writer.writeSync(42), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => writer.writeSync({}), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => writer.writeSync(true), { code: 'ERR_INVALID_ARG_TYPE' });
  writer.endSync();
}

// =============================================================================
// duplex() validation
// =============================================================================

assert.throws(() => duplex(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => duplex('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => duplex({ a: 42 }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => duplex({ b: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });

// highWaterMark validation (cascades through to push())
assert.throws(() => duplex({ highWaterMark: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => duplex({ highWaterMark: 1.5 }), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => duplex({ highWaterMark: Number.MAX_SAFE_INTEGER + 1 }),
              { code: 'ERR_OUT_OF_RANGE' });

// Values < 1 are clamped to 1 (both directions)
{
  const [a, b] = duplex({ highWaterMark: 0 });
  assert.strictEqual(a.writer.desiredSize, 1);
  assert.strictEqual(b.writer.desiredSize, 1);
  a.close();
  b.close();
}
// MAX_SAFE_INTEGER is accepted
{
  const [a, b] = duplex({ highWaterMark: Number.MAX_SAFE_INTEGER });
  assert.strictEqual(a.writer.desiredSize, Number.MAX_SAFE_INTEGER);
  assert.strictEqual(b.writer.desiredSize, Number.MAX_SAFE_INTEGER);
  a.close();
  b.close();
}
// Per-direction overrides
{
  const [a, b] = duplex({ a: { highWaterMark: 0 }, b: { highWaterMark: 5 } });
  assert.strictEqual(a.writer.desiredSize, 1); // clamped
  assert.strictEqual(b.writer.desiredSize, 5);
  a.close();
  b.close();
}

assert.throws(() => duplex({ signal: {} }), { code: 'ERR_INVALID_ARG_TYPE' });

// =============================================================================
// pull() / pullSync() validation
// =============================================================================

// Signal must be AbortSignal
assert.throws(() => pull(from('a'), { signal: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });

// Transforms must be functions or transform objects
assert.throws(() => pull(from('a'), 42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => pull(from('a'), 'bad'), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => pullSync(fromSync('a'), 42), { code: 'ERR_INVALID_ARG_TYPE' });

// =============================================================================
// broadcast() validation
// =============================================================================

assert.throws(() => broadcast({ highWaterMark: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => broadcast({ highWaterMark: 1.5 }), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => broadcast({ highWaterMark: Number.MAX_SAFE_INTEGER + 1 }),
              { code: 'ERR_OUT_OF_RANGE' });

// Values < 1 are clamped to 1 (need a consumer for desiredSize to work)
{
  const bc = broadcast({ highWaterMark: 0 });
  bc.broadcast.push();
  assert.strictEqual(bc.writer.desiredSize, 1);
  bc.writer.endSync();
}
{
  const bc = broadcast({ highWaterMark: -1 });
  bc.broadcast.push();
  assert.strictEqual(bc.writer.desiredSize, 1);
  bc.writer.endSync();
}
// MAX_SAFE_INTEGER is accepted
{
  const bc = broadcast({ highWaterMark: Number.MAX_SAFE_INTEGER });
  bc.broadcast.push();
  assert.strictEqual(bc.writer.desiredSize, Number.MAX_SAFE_INTEGER);
  bc.writer.endSync();
}

assert.throws(() => broadcast({ signal: {} }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => broadcast({ backpressure: 'bad' }), { code: 'ERR_INVALID_ARG_VALUE' });

// Broadcast.from rejects non-iterable input
assert.throws(() => Broadcast.from(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => Broadcast.from('bad'), { code: 'ERR_INVALID_ARG_TYPE' });

// =============================================================================
// share() / shareSync() validation
// =============================================================================

assert.throws(() => share(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => share(from('a'), { highWaterMark: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => share(from('a'), { highWaterMark: 1.5 }), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => share(from('a'), { highWaterMark: Number.MAX_SAFE_INTEGER + 1 }),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => share(from('a'), { signal: {} }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => share(from('a'), { backpressure: 'bad' }), { code: 'ERR_INVALID_ARG_VALUE' });

// share() values < 1 are clamped (no desiredSize, but accepts the value)
share(from('a'), { highWaterMark: 0 }).cancel();
share(from('a'), { highWaterMark: -1 }).cancel();
share(from('a'), { highWaterMark: Number.MAX_SAFE_INTEGER }).cancel();

assert.throws(() => shareSync(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => shareSync(fromSync('a'), { highWaterMark: 'bad' }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => shareSync(fromSync('a'), { highWaterMark: 1.5 }),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => shareSync(fromSync('a'), { highWaterMark: Number.MAX_SAFE_INTEGER + 1 }),
              { code: 'ERR_OUT_OF_RANGE' });

// shareSync() values < 1 are clamped (accepts the value)
shareSync(fromSync('a'), { highWaterMark: 0 }).cancel();
shareSync(fromSync('a'), { highWaterMark: -1 }).cancel();
shareSync(fromSync('a'), { highWaterMark: Number.MAX_SAFE_INTEGER }).cancel();

// Share.from / SyncShare.fromSync reject non-iterable
assert.throws(() => Share.from(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => SyncShare.fromSync(42), { code: 'ERR_INVALID_ARG_TYPE' });

// =============================================================================
// Consumer validation (synchronous)
// =============================================================================

// tap / tapSync require function
assert.throws(() => tap(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => tap('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => tapSync(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => tapSync(null), { code: 'ERR_INVALID_ARG_TYPE' });

// Sync consumer options
assert.throws(() => bytesSync(fromSync('a'), { limit: 'bad' }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bytesSync(fromSync('a'), { limit: -1 }),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => textSync(fromSync('a'), { encoding: 42 }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => textSync(fromSync('a'), { encoding: 'bogus' }),
              { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => arrayBufferSync(fromSync('a'), { limit: 'bad' }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => arraySync(fromSync('a'), { limit: -1 }),
              { code: 'ERR_OUT_OF_RANGE' });

// Options must be object if provided
assert.throws(() => bytesSync(fromSync('a'), 42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => textSync(fromSync('a'), 'bad'), { code: 'ERR_INVALID_ARG_TYPE' });

// Compression options must be object
assert.throws(() => compressGzip(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => decompressGzip('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => compressBrotli(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => decompressBrotli('bad'), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => compressZstd(42), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => decompressZstd('bad'), { code: 'ERR_INVALID_ARG_TYPE' });

// =============================================================================
// Async consumer and compression validation
// =============================================================================

// Helper: consume a transform through a pipeline to trigger lazy validation.
const consume = (transform) => bytes(pull(from('test'), transform));

async function testAsyncValidation() {
  // pipeTo signal
  await assert.rejects(
    () => pipeTo(from('a'), { write() {} }, { signal: 'bad' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  // Async consumer options
  await assert.rejects(
    () => bytes(from('a'), 42), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(
    () => bytes(from('a'), { signal: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(
    () => bytes(from('a'), { limit: 'bad' }), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(
    () => bytes(from('a'), { limit: -1 }), { code: 'ERR_OUT_OF_RANGE' });
  await assert.rejects(
    () => text(from('a'), { encoding: 42 }), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(
    () => text(from('a'), { encoding: 'not-a-real-encoding' }),
    { code: 'ERR_INVALID_ARG_VALUE' });
  await assert.rejects(
    () => arrayBuffer(from('a'), { limit: 'bad' }),
    { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(
    () => array(from('a'), { limit: -1 }), { code: 'ERR_OUT_OF_RANGE' });

  const TYPE = { code: 'ERR_INVALID_ARG_TYPE' };
  const RANGE = { code: 'ERR_OUT_OF_RANGE' };
  const BROTLI = { code: 'ERR_BROTLI_INVALID_PARAM' };
  const ZSTD = { code: 'ERR_ZSTD_INVALID_PARAM' };

  // ChunkSize
  await assert.rejects(consume(compressGzip({ chunkSize: 'bad' })), TYPE);
  await assert.rejects(consume(compressGzip({ chunkSize: 0 })), RANGE);
  await assert.rejects(consume(compressGzip({ chunkSize: 10 })), RANGE);

  // WindowBits
  await assert.rejects(consume(compressGzip({ windowBits: 'bad' })), TYPE);
  await assert.rejects(consume(compressGzip({ windowBits: 100 })), RANGE);

  // Level
  await assert.rejects(consume(compressGzip({ level: 'bad' })), TYPE);
  await assert.rejects(consume(compressGzip({ level: 100 })), RANGE);

  // MemLevel
  await assert.rejects(consume(compressGzip({ memLevel: 'bad' })), TYPE);
  await assert.rejects(consume(compressGzip({ memLevel: 100 })), RANGE);

  // Strategy
  await assert.rejects(consume(compressGzip({ strategy: 'bad' })), TYPE);
  await assert.rejects(consume(compressGzip({ strategy: 100 })), RANGE);

  // Dictionary
  await assert.rejects(consume(compressGzip({ dictionary: 42 })), TYPE);
  await assert.rejects(consume(compressGzip({ dictionary: 'bad' })), TYPE);

  // Brotli params
  await assert.rejects(consume(compressBrotli({ params: 42 })), TYPE);
  await assert.rejects(consume(compressBrotli({ params: { bad: 1 } })), BROTLI);
  await assert.rejects(consume(compressBrotli({ params: { [-1]: 1 } })), BROTLI);
  await assert.rejects(consume(compressBrotli({ params: { 0: 'bad' } })), TYPE);

  // Zstd params
  await assert.rejects(consume(compressZstd({ params: 42 })), TYPE);
  await assert.rejects(consume(compressZstd({ params: { bad: 1 } })), ZSTD);
  await assert.rejects(consume(compressZstd({ params: { 0: 'bad' } })), TYPE);

  // Zstd pledgedSrcSize
  await assert.rejects(consume(compressZstd({ pledgedSrcSize: 'bad' })), TYPE);
  await assert.rejects(consume(compressZstd({ pledgedSrcSize: -1 })), RANGE);
}

// =============================================================================
// Valid calls still work
// =============================================================================

// Push with valid options
{
  const { writer } = push({ highWaterMark: 2 });
  writer.writeSync('hello');
  writer.endSync();
}

// Duplex with valid options
{
  const [a, b] = duplex({ highWaterMark: 2 });
  a.close();
  b.close();
}

// Broadcast with valid options
{
  const { writer } = broadcast({ highWaterMark: 4 });
  writer.endSync();
}

// Share with valid options
{
  const shared = share(from('hello'), { highWaterMark: 4 });
  shared.cancel();
}

// Compression with valid options
{
  const transform = compressGzip({ chunkSize: 1024, level: 6 });
  assert.strictEqual(typeof transform.transform, 'function');
}

// Brotli with valid params
{
  const { constants: { BROTLI_PARAM_QUALITY } } = require('zlib');
  const transform = compressBrotli({ params: { [BROTLI_PARAM_QUALITY]: 5 } });
  assert.strictEqual(typeof transform.transform, 'function');
}

testAsyncValidation().then(common.mustCall());
