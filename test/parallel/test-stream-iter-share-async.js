// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  share,
  text,
} = require('stream/iter');

const { setTimeout } = require('timers/promises');

// =============================================================================
// Async share()
// =============================================================================

async function testBasicShare() {
  const shared = share(from('hello shared'));

  const consumer = shared.pull();
  const data = await text(consumer);
  assert.strictEqual(data, 'hello shared');
}

async function testShareMultipleConsumers() {
  async function* gen() {
    yield [new TextEncoder().encode('chunk1')];
    yield [new TextEncoder().encode('chunk2')];
    yield [new TextEncoder().encode('chunk3')];
  }

  const shared = share(gen(), { highWaterMark: 16 });

  const c1 = shared.pull();
  const c2 = shared.pull();

  assert.strictEqual(shared.consumerCount, 2);

  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);

  assert.strictEqual(data1, 'chunk1chunk2chunk3');
  assert.strictEqual(data2, 'chunk1chunk2chunk3');
}

async function testShareConsumerCount() {
  const shared = share(from('data'));

  assert.strictEqual(shared.consumerCount, 0);

  const c1 = shared.pull();
  assert.strictEqual(shared.consumerCount, 1);

  const c2 = shared.pull();
  assert.strictEqual(shared.consumerCount, 2);

  // Cancel detaches all consumers
  shared.cancel();
  assert.strictEqual(shared.consumerCount, 0);

  // Both should complete immediately
  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);
  assert.strictEqual(data1, '');
  assert.strictEqual(data2, '');
}

async function testShareCancel() {
  const shared = share(from('data'));
  const consumer = shared.pull();

  shared.cancel();
  assert.strictEqual(shared.consumerCount, 0);

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testShareCancelMidIteration() {
  // Verify that cancel during iteration stops data flow
  let sourceReturnCalled = false;
  const enc = new TextEncoder();
  async function* gen() {
    try {
      yield [enc.encode('a')];
      yield [enc.encode('b')];
      yield [enc.encode('c')];
    } finally {
      sourceReturnCalled = true;
    }
  }
  const shared = share(gen(), { highWaterMark: 16 });
  const consumer = shared.pull();

  const items = [];
  for await (const batch of consumer) {
    for (const chunk of batch) {
      items.push(new TextDecoder().decode(chunk));
    }
    // Cancel after first batch
    shared.cancel();
  }
  assert.strictEqual(items.length, 1);
  assert.strictEqual(items[0], 'a');

  await new Promise(setImmediate);
  assert.strictEqual(sourceReturnCalled, true);
}

async function testShareCancelWithReason() {
  const shared = share(from('data'));
  const consumer = shared.pull();

  shared.cancel(new Error('share cancelled'));

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of consumer) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'share cancelled' },
  );
}

async function testShareAbortSignal() {
  const ac = new AbortController();
  const shared = share(from('data'), { signal: ac.signal });
  const consumer = shared.pull();

  ac.abort();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testShareAlreadyAborted() {
  const ac = new AbortController();
  ac.abort();

  const shared = share(from('data'), { signal: ac.signal });
  const consumer = shared.pull();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// Source error propagation
// =============================================================================

async function testShareSourceError() {
  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('share source boom');
  }
  const shared = share(failingSource());
  const c1 = shared.pull();
  const c2 = shared.pull();

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of c1) { /* consume */ }
  }, { message: 'share source boom' });
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of c2) { /* consume */ }
  }, { message: 'share source boom' });
}

async function testShareLateJoiningConsumer() {
  // A consumer that joins after some data has been consumed should only
  // see data remaining in the buffer (not items already trimmed).
  const enc = new TextEncoder();
  async function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }
  const shared = share(gen(), { highWaterMark: 16 });

  // First consumer reads all data
  const c1 = shared.pull();
  const data1 = await text(c1);
  assert.strictEqual(data1, 'abc');

  // Late-joining consumer: source is exhausted, buffer has been trimmed
  // past all data by c1's reads, so c2 gets nothing.
  const c2 = shared.pull();
  const data2 = await text(c2);
  assert.strictEqual(data2, '');
}

async function testShareConsumerBreak() {
  // Verify that a consumer breaking mid-iteration detaches properly
  const enc = new TextEncoder();
  async function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }
  const shared = share(gen(), { highWaterMark: 16 });
  const c1 = shared.pull();
  const c2 = shared.pull();

  assert.strictEqual(shared.consumerCount, 2);

  // c1 breaks after first batch
  // eslint-disable-next-line no-unused-vars
  for await (const _ of c1) {
    break;
  }
  // c1 should be detached
  assert.strictEqual(shared.consumerCount, 1);

  // c2 should still get all data
  const data2 = await text(c2);
  assert.strictEqual(data2, 'abc');
}

async function testShareMultipleConsumersConcurrentPull() {
  // Multiple consumers pulling concurrently should each receive all items
  // even when only one item is pulled from source at a time.
  async function* slowSource() {
    const enc = new TextEncoder();
    for (let i = 0; i < 5; i++) {
      await setTimeout(1);
      yield [enc.encode(`item-${i}`)];
    }
  }
  const shared = share(slowSource());
  const c1 = shared.pull();
  const c2 = shared.pull();
  const c3 = shared.pull();

  const [t1, t2, t3] = await Promise.all([
    text(c1), text(c2), text(c3),
  ]);

  const expected = 'item-0item-1item-2item-3item-4';
  assert.strictEqual(t1, expected);
  assert.strictEqual(t2, expected);
  assert.strictEqual(t3, expected);
}

// share() accepts string source directly (normalized via from())
async function testShareStringSource() {
  const shared = share('hello-share');
  const result = await text(shared.pull());
  assert.strictEqual(result, 'hello-share');
}

Promise.all([
  testBasicShare(),
  testShareMultipleConsumers(),
  testShareConsumerCount(),
  testShareCancel(),
  testShareCancelMidIteration(),
  testShareCancelWithReason(),
  testShareAbortSignal(),
  testShareAlreadyAborted(),
  testShareSourceError(),
  testShareLateJoiningConsumer(),
  testShareConsumerBreak(),
  testShareMultipleConsumersConcurrentPull(),
  testShareStringSource(),
]).then(common.mustCall());
