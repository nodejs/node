// Flags: --allow-natives-syntax
'use strict';

// Checks that profiles encode to the same gzip-compressed pprof protobuf
// message synchronously and asynchronously.

const common = require('../common');
const assert = require('node:assert');
const zlib = require('node:zlib');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encode, encodeSync } = require('node:perf_hooks/pprof');

// Profiles lack inlined frames of optimized code that was compiled before the
// profiler started: see pprofTestBusyLoop() in test/common/pprof.js.
eval('%NeverOptimizeFunction(pprofTestBusyLoop)');

time.start();
pprofTestBusyLoop(100_000);
const profile = time.stop();

const encoded = encodeSync(profile);
assert.ok(Buffer.isBuffer(encoded));
// gzip magic number.
assert.strictEqual(encoded[0], 0x1f);
assert.strictEqual(encoded[1], 0x8b);
assert.deepStrictEqual(zlib.gunzipSync(encoded), profile.encode());

const decoded = decodeGzippedProfile(encoded);
assert.strictEqual(decoded.strings[0], '');
assert.ok(decoded.samples.some(({ stack }) =>
  stack.some(({ name }) => name === 'pprofTestBusyLoop')));

const promise = encode(profile);
assert.ok(promise instanceof Promise);
promise.then(async (asyncEncoded) => {
  assert.ok(Buffer.isBuffer(asyncEncoded));
  assert.deepStrictEqual(zlib.gunzipSync(asyncEncoded), profile.encode());
  assert.deepStrictEqual(await profile.encodeAsync(), profile.encode());
}).then(common.mustCall());
