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

  const shared = shareSync(gen(), { highWaterMark: 16 });

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
  const shared = shareSync(gen(), { highWaterMark: 16 });
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
  // When cancel(reason) is called, a consumer that hasn't started
  // iterating is already detached, so it sees done:true (not the error).
  // But a consumer that is mid-iteration when another consumer cancels
  // with a reason will see the error on the next pull after cancel.
  const enc = new TextEncoder();
  function* gen() {
    yield [enc.encode('a')];
    yield [enc.encode('b')];
    yield [enc.encode('c')];
  }
  const shared = shareSync(gen(), { highWaterMark: 16 });
  const c1 = shared.pull();
  const c2 = shared.pull();

  // c1 reads one item, then c2 cancels with a reason
  const iter1 = c1[Symbol.iterator]();
  const first = iter1.next();
  assert.strictEqual(first.done, false);

  shared.cancel(new Error('sync cancel reason'));

  // c1 was already iterating, it's now detached → done
  const next = iter1.next();
  assert.strictEqual(next.done, true);

  // c2 never started, also detached → done (not error)
  const batches = [];
  for (const batch of c2) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
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
  testShareSyncSourceError(),
  testShareSyncStringSource(),
]).then(common.mustCall());
