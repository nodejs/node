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
  testShareSyncStringSource(),
]).then(common.mustCall());
