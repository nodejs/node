// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  pull,
  pullSync,
  push,
  tap,
  tapSync,
  text,
  textSync,
} = require('stream/iter');

// =============================================================================
// tap / tapSync
// =============================================================================

async function testTapSync() {
  const observed = [];
  const observer = tapSync((chunks) => {
    if (chunks !== null) {
      observed.push(chunks.length);
    }
  });

  // tapSync returns a function transform
  assert.strictEqual(typeof observer, 'function');

  // Test that it passes data through unchanged
  const input = [new Uint8Array([1]), new Uint8Array([2])];
  const result = observer(input);
  assert.deepStrictEqual(result, input);
  assert.deepStrictEqual(observed, [2]);

  // null (flush) passes through
  const flushResult = observer(null);
  assert.strictEqual(flushResult, null);
}

async function testTapAsync() {
  const observed = [];
  const observer = tap(async (chunks) => {
    if (chunks !== null) {
      observed.push(chunks.length);
    }
  });

  assert.strictEqual(typeof observer, 'function');

  const input = [new Uint8Array([1])];
  const result = await observer(input);
  assert.deepStrictEqual(result, input);
  assert.deepStrictEqual(observed, [1]);
}

async function testTapInPipeline() {
  const { writer, readable } = push();
  const seen = [];

  const observer = tap(async (chunks) => {
    if (chunks !== null) {
      for (const chunk of chunks) {
        seen.push(new TextDecoder().decode(chunk));
      }
    }
  });

  writer.write('hello');
  writer.end();

  // Use pull with tap as a transform
  const result = pull(readable, observer);
  const data = await text(result);

  assert.strictEqual(data, 'hello');
  assert.strictEqual(seen.length, 1);
  assert.strictEqual(seen[0], 'hello');
}

// Tap callback error propagates through async pipeline
async function testTapAsyncErrorPropagation() {
  const badTap = tap(() => { throw new Error('tap error'); });
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of pull(from('hello'), badTap)) { /* consume */ }
  }, { message: 'tap error' });
}

// TapSync callback error propagates through sync pipeline
function testTapSyncErrorPropagation() {
  const badTap = tapSync(() => { throw new Error('tapSync error'); });
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(fromSync('hello'), badTap)) { /* consume */ }
  }, { message: 'tapSync error' });
}

// TapSync in a pullSync pipeline passes through data and flush
function testTapSyncInPipeline() {
  const seen = [];
  let sawFlush = false;
  const observer = tapSync((chunks) => {
    if (chunks === null) {
      sawFlush = true;
    } else {
      for (const chunk of chunks) {
        seen.push(new TextDecoder().decode(chunk));
      }
    }
  });

  const data = textSync(pullSync(fromSync('hello'), observer));
  assert.strictEqual(data, 'hello');
  assert.strictEqual(seen.length, 1);
  assert.strictEqual(seen[0], 'hello');
  assert.strictEqual(sawFlush, true);
}

Promise.all([
  testTapSync(),
  testTapAsync(),
  testTapInPipeline(),
  testTapAsyncErrorPropagation(),
  testTapSyncErrorPropagation(),
  testTapSyncInPipeline(),
]).then(common.mustCall());
