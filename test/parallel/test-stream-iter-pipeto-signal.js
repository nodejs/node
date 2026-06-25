// Flags: --experimental-stream-iter
'use strict';

// Tests for pipeTo with live (non-pre-aborted) AbortSignal,
// both with and without transforms.

const common = require('../common');
const assert = require('assert');
const { setTimeout } = require('timers/promises');
const { pipeTo, from } = require('stream/iter');

// pipeTo with live signal, no transforms — abort mid-stream
async function testPipeToLiveSignalNoTransforms() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() {},
  };
  async function* source() {
    yield [new Uint8Array([1])];
    yield [new Uint8Array([2])];
    ac.abort();
    yield [new Uint8Array([3])];
  }
  await assert.rejects(
    () => pipeTo(source(), writer, { signal: ac.signal }),
    { name: 'AbortError' },
  );
  // Should have written at least the first two chunks before abort
  assert.ok(written.length >= 1);
}

// pipeTo with live signal, no transforms — abort while waiting for next chunk
async function testPipeToLiveSignalNoTransformsPendingNext() {
  const ac = new AbortController();
  const reason = new Error('abort reason');
  const writer = {
    write: common.mustNotCall(),
  };
  const source = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          return new Promise(() => {});
        },
      };
    },
  };

  setTimeout(10)
    .then(() => ac.abort(reason))
    .then(common.mustCall());

  const result = await Promise.race([
    assert.rejects(
      () => pipeTo(source, writer, { signal: ac.signal }),
      reason,
    ).then(() => 'aborted'),
    setTimeout(1000, 'timed out'),
  ]);

  assert.strictEqual(result, 'aborted');
}

// pipeTo with live signal + transforms — abort mid-stream
async function testPipeToLiveSignalWithTransforms() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() {},
  };
  const identity = (chunks) => chunks;
  async function* source() {
    yield [new Uint8Array([10])];
    yield [new Uint8Array([20])];
    ac.abort();
    yield [new Uint8Array([30])];
  }
  await assert.rejects(
    () => pipeTo(source(), identity, writer, { signal: ac.signal }),
    { name: 'AbortError' },
  );
  assert.ok(written.length >= 1);
}

// pipeTo with live signal, no abort — runs to completion
async function testPipeToLiveSignalCompletes() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    write(chunk) { written.push(chunk); },
    writeSync(chunk) { written.push(chunk); return true; },
    async end() {},
    endSync() { return written.length; },
  };
  await pipeTo(from('signal-ok'), writer, { signal: ac.signal });
  assert.ok(written.length > 0);
}

// pipeTo with live signal + transforms, no abort — runs to completion
async function testPipeToLiveSignalWithTransformsCompletes() {
  const ac = new AbortController();
  const written = [];
  const writer = {
    write(chunk) { written.push(chunk); },
    writeSync(chunk) { written.push(chunk); return true; },
    async end() {},
    endSync() { return written.length; },
  };
  const identity = (chunks) => chunks;
  await pipeTo(from('signal-tx-ok'), identity, writer,
               { signal: ac.signal });
  assert.ok(written.length > 0);
}

Promise.all([
  testPipeToLiveSignalNoTransforms(),
  testPipeToLiveSignalNoTransformsPendingNext(),
  testPipeToLiveSignalWithTransforms(),
  testPipeToLiveSignalCompletes(),
  testPipeToLiveSignalWithTransformsCompletes(),
]).then(common.mustCall());
