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

function testShareSyncRejectsDropNewest() {
  // A synchronous consumer can neither wait for the slowest consumer nor keep
  // discarding until it advances, so 'drop-newest' is rejected like
  // 'unbounded'.
  assert.throws(
    () => shareSync(fromSync('data'), { backpressure: 'drop-newest' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}

// shareSync() accepts string source directly (normalized via fromSync())
function testShareSyncRetainsBufferWhenAllConsumersDetach() {
  // Data that a consumer had not read yet must stay available to consumers
  // that attach after every previous consumer has detached.
  const enc = new TextEncoder();
  function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }
  const shared = shareSync(gen(), { budget: 16384 });
  const c1 = shared.pull()[Symbol.iterator]();
  const c2 = shared.pull()[Symbol.iterator]();
  assert.deepStrictEqual(c1.next().value, [enc.encode('a')]);
  c1.return();
  c2.return();
  assert.strictEqual(shared.consumerCount, 0);

  assert.strictEqual(textSync(shared.pull()), 'abc');
}

function testShareSyncStrictBackpressureDetaches() {
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
    const fast = consumer[Symbol.iterator]();
    // This consumer prevents the buffer from being trimmed.
    const slow = shared.pull()[Symbol.iterator]();

    fast.next();
    fast.next();
    assert.throws(() => fast.next(), { code: 'ERR_OUT_OF_RANGE' });
    // The rejected consumer is detached, as with the async share.
    assert.strictEqual(shared.consumerCount, 1);
    assert.strictEqual(fast.next().done, true);

    // The detached consumer no longer pins the buffer, so the remaining
    // consumer can read the whole source.
    let count = 0;
    while (!slow.next().done) count++;
    assert.strictEqual(count, 10);
  }
}

function testShareSyncStrictForOfDoesNotWedgeOthers() {
  // for...of does not call return() when next() throws. The consumer that
  // hit the budget must still not keep the other consumers from reading.
  function* source() {
    for (let i = 0; i < 20; i++) yield [new Uint8Array(8192)];
  }
  const shared = shareSync(source(), { budget: 16384, backpressure: 'strict' });
  const slow = shared.pull()[Symbol.iterator]();
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of shared.pull()) { /* consume */ }
  }, { code: 'ERR_OUT_OF_RANGE' });
  assert.strictEqual(shared.consumerCount, 1);
  let count = 0;
  while (!slow.next().done) count++;
  assert.strictEqual(count, 20);
}

function testShareSyncDropOldestSplitsOversizedBatches() {
  // fromSync() combines the values of this generator into a single batch
  // that is much larger than the budget. Evicting that batch as a whole would
  // leave the slower consumer with nothing at all.
  function* source() {
    for (let i = 0; i < 50; i++) {
      const chunk = new Uint8Array(4096);
      chunk[0] = i;
      yield chunk;
    }
  }
  const shared = shareSync(source(), {
    budget: 65536,
    backpressure: 'drop-oldest',
  });
  const fast = shared.pull()[Symbol.iterator]();
  const slow = shared.pull()[Symbol.iterator]();

  const fastSeen = [];
  for (let r = fast.next(); !r.done; r = fast.next()) {
    for (const chunk of r.value) fastSeen.push(chunk[0]);
  }
  assert.deepStrictEqual(fastSeen, Array.from({ length: 50 }, (_, i) => i));

  const slowSeen = [];
  for (let r = slow.next(); !r.done; r = slow.next()) {
    for (const chunk of r.value) slowSeen.push(chunk[0]);
  }
  // The slow consumer lost the oldest chunks but keeps an in-order suffix
  // that fits the budget.
  assert.ok(slowSeen.length > 0);
  assert.ok(slowSeen.length * 4096 < 65536);
  assert.deepStrictEqual(
    slowSeen,
    Array.from({ length: slowSeen.length }, (_, i) => 50 - slowSeen.length + i));
}

function testShareSyncReentrantSourceRead() {
  const enc = new TextEncoder();
  let sibling;
  let reentrantError;
  function* source() {
    yield [enc.encode('a')];
    try {
      sibling.next();
    } catch (err) {
      reentrantError = err;
    }
    yield [enc.encode('b')];
  }
  const shared = shareSync(source());
  const c1 = shared.pull()[Symbol.iterator]();
  sibling = shared.pull()[Symbol.iterator]();

  assert.deepStrictEqual(c1.next().value, [enc.encode('a')]);
  assert.deepStrictEqual(sibling.next().value, [enc.encode('a')]);
  // Pulling 'b' runs the source, which tries to read its own share.
  assert.deepStrictEqual(c1.next().value, [enc.encode('b')]);
  assert.strictEqual(reentrantError?.code, 'ERR_INVALID_STATE');
  // The failed re-entrant read left the share intact.
  assert.deepStrictEqual(sibling.next().value, [enc.encode('b')]);
  assert.strictEqual(c1.next().done, true);
  assert.strictEqual(sibling.next().done, true);
}

function testShareSyncReentrantSourceReadUncaught() {
  let sibling;
  function* source() {
    yield [new Uint8Array(1)];
    sibling.next();
  }
  const shared = shareSync(source());
  const c1 = shared.pull()[Symbol.iterator]();
  sibling = shared.pull()[Symbol.iterator]();
  c1.next();
  sibling.next();
  // The error escapes the source, so it becomes the share's source error.
  assert.throws(() => c1.next(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => sibling.next(), { code: 'ERR_INVALID_STATE' });
}

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
  testShareSyncRejectsDropNewest(),
  testShareSyncDropOldestSplitsOversizedBatches(),
  testShareSyncReentrantSourceRead(),
  testShareSyncReentrantSourceReadUncaught(),
  testShareSyncStringSource(),
  testShareSyncRetainsBufferWhenAllConsumersDetach(),
  testShareSyncStrictBackpressureDetaches(),
  testShareSyncStrictForOfDoesNotWedgeOthers(),
]).then(common.mustCall());
