// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  shareSync,
  fromSync,
  textSync,

} = require('stream/iter');

// =============================================================================
// Sync share
// =============================================================================

async function testShareSyncBasic() {
  const shared = shareSync(fromSync('sync shared'));

  const consumer = shared.pull();
  const data = textSync(consumer);
  assert.strictEqual(data, 'sync shared');
}

async function testShareSyncMultipleConsumers() {
  const enc = new TextEncoder();
  function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }

  const shared = shareSync(gen(), { budget: 16384 });

  const c1 = shared.pull();
  const c2 = shared.pull();

  const data1 = textSync(c1);
  const data2 = textSync(c2);

  assert.strictEqual(data1, 'abc');
  assert.strictEqual(data2, 'abc');
}

function testShareSyncCancel() {
  // Verify that cancel() on a pre-iteration share yields nothing
  const shared = shareSync(fromSync('data'));
  const consumer = shared.pull();

  shared.cancel();
  assert.strictEqual(shared.consumerCount, 0);

  const batches = [];
  for (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

function testShareSyncCancelMidIteration() {
  // Verify cancel during iteration stops data flow and cleans up
  const enc = new TextEncoder();
  let sourceReturnCalled = false;
  function* gen() {
    try {
      yield [enc.encode('a')];
      yield [enc.encode('b')];
      yield [enc.encode('c')];
    } finally {
      sourceReturnCalled = true;
    }
  }
  const shared = shareSync(gen(), { budget: 16384 });
  const consumer = shared.pull();

  const items = [];
  for (const batch of consumer) {
    for (const chunk of batch) {
      items.push(new TextDecoder().decode(chunk));
    }
    // Cancel after first batch
    shared.cancel();
  }
  assert.strictEqual(items.length, 1);
  assert.strictEqual(items[0], 'a');
  assert.strictEqual(sourceReturnCalled, true);
}

function testShareSyncCancelWithReason() {
  const enc = new TextEncoder();
  function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
  }
  const shared = shareSync(gen(), { budget: 16384 });
  const iterator1 = shared.pull()[Symbol.iterator]();
  const iterator2 = shared.pull()[Symbol.iterator]();
  const reason = new Error('sync cancel reason');

  iterator1.next();
  shared.cancel(reason);

  assert.throws(() => iterator1.next(), (error) => error === reason);
  assert.throws(() => iterator2.next(), (error) => error === reason);
}

function testShareSyncCancelWithFalsyReason() {
  for (const reason of [0, '', false, null]) {
    const shared = shareSync(fromSync('data'));
    const iterator = shared.pull()[Symbol.iterator]();

    shared.cancel(reason);

    assert.throws(() => iterator.next(), (error) => error === reason);
  }
}

// =============================================================================
// Source error propagation
// =============================================================================

function testShareSyncSourceError() {
  function* failingSource() {
    yield [new TextEncoder().encode('ok')];
    throw new Error('sync share boom');
  }
  const shared = shareSync(failingSource());
  const c1 = shared.pull();
  const c2 = shared.pull();

  // Both consumers should see the error
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of c1) { /* consume */ }
  }, { message: 'sync share boom' });
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of c2) { /* consume */ }
  }, { message: 'sync share boom' });
}

function testShareSyncRejectsUnbounded() {
  assert.throws(
    () => shareSync(fromSync('data'), { backpressure: 'unbounded' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}

function testShareSyncDropNewest() {
  let pulls = 0;
  function* source() {
    for (let i = 0; i < 4; i++) {
      pulls++;
      const chunk = new Uint8Array(16384);
      chunk[0] = i;
      yield [chunk];
    }
  }

  const shared = shareSync(source(), {
    budget: 16384,
    backpressure: 'drop-newest',
  });
  const fast = shared.pull()[Symbol.iterator]();
  const slow = shared.pull()[Symbol.iterator]();

  assert.strictEqual(fast.next().value[0][0], 0);

  // The budget is exhausted and the slow consumer cannot advance while this
  // call is running, so exactly one entry is dropped and no value is
  // available. The consumer is not detached.
  assert.strictEqual(fast.next().done, true);
  assert.strictEqual(pulls, 2);

  // The slow consumer still sees the buffered entry, which releases budget.
  assert.strictEqual(slow.next().value[0][0], 0);

  // Entry 1 was dropped for every consumer, so both resume at entry 2.
  assert.strictEqual(slow.next().value[0][0], 2);
  assert.strictEqual(pulls, 3);
  assert.strictEqual(fast.next().value[0][0], 2);
}

// Regression test: a full buffer must not spin pulling-and-discarding from an
// unbounded source, since discarding never reclaims budget.
function testShareSyncDropNewestUnboundedSource() {
  let pulls = 0;
  function* source() {
    for (;;) {
      pulls++;
      yield [new Uint8Array(16384)];
    }
  }

  const shared = shareSync(source(), {
    budget: 16384,
    backpressure: 'drop-newest',
  });
  const fast = shared.pull()[Symbol.iterator]();
  shared.pull();

  assert.strictEqual(fast.next().done, false);
  assert.strictEqual(pulls, 1);

  // Each blocked call drops at most one entry and returns without a value.
  for (let i = 0; i < 3; i++) {
    assert.strictEqual(fast.next().done, true);
    assert.strictEqual(pulls, 2 + i);
  }

  shared.cancel();
}

// A strict budget failure detaches the consumer that threw so it cannot
// later pin the buffer for consumers that continue reading.
function testShareSyncStrictBackpressure() {
  for (const transformed of [false, true]) {
    function* source() {
      for (let i = 0; i < 10; i++) {
        yield [new Uint8Array(16384)];
      }
    }

    const shared = shareSync(source(), {
      budget: 32768,
      backpressure: 'strict',
    });

    const consumer = transformed ?
      shared.pull((chunks) => chunks) : shared.pull();
    const failed = consumer[Symbol.iterator]();
    const live = shared.pull()[Symbol.iterator]();

    assert.strictEqual(failed.next().done, false);
    assert.strictEqual(failed.next().done, false);
    assert.throws(() => failed.next(), {
      code: 'ERR_OUT_OF_RANGE',
    });

    assert.strictEqual(shared.consumerCount, 1);
    assert.strictEqual(failed.next().done, true);

    let count = 0;
    while (!live.next().done) {
      count++;
    }
    assert.strictEqual(count, 10);
    assert.strictEqual(shared.consumerCount, 0);
  }
}

// shareSync() accepts string source directly (normalized via fromSync())
function testShareSyncStringSource() {
  const shared = shareSync('hello-sync-share');
  const result = textSync(shared.pull());
  assert.strictEqual(result, 'hello-sync-share');
}

Promise.all([
  testShareSyncBasic(),
  testShareSyncMultipleConsumers(),
  testShareSyncCancel(),
  testShareSyncCancelMidIteration(),
  testShareSyncCancelWithReason(),
  testShareSyncCancelWithFalsyReason(),
  testShareSyncSourceError(),
  testShareSyncRejectsUnbounded(),
  testShareSyncDropNewest(),
  testShareSyncDropNewestUnboundedSource(),
  testShareSyncStrictBackpressure(),
  testShareSyncStringSource(),
]).then(common.mustCall());
