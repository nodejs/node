'use strict';

const common = require('../common');
const assert = require('assert');
const {
  share,
  shareSync,
  Share,
  SyncShare,
  from,
  fromSync,
  text,
  textSync,

} = require('stream/new');

// =============================================================================
// Async share()
// =============================================================================

async function testBasicShare() {
  const source = from('hello shared');
  const shared = share(source);

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
  const source = from('data');
  const shared = share(source);

  assert.strictEqual(shared.consumerCount, 0);

  const c1 = shared.pull();
  assert.strictEqual(shared.consumerCount, 1);

  const c2 = shared.pull();
  assert.strictEqual(shared.consumerCount, 2);

  // Cancel detaches all consumers
  shared.cancel();

  // Both should complete immediately
  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);
  assert.strictEqual(data1, '');
  assert.strictEqual(data2, '');
}

async function testShareCancel() {
  const source = from('data');
  const shared = share(source);
  const consumer = shared.pull();

  shared.cancel();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testShareCancelWithReason() {
  const source = from('data');
  const shared = share(source);
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
  const source = from('data');
  const shared = share(source, { signal: ac.signal });
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

  const source = from('data');
  const shared = share(source, { signal: ac.signal });
  const consumer = shared.pull();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// Share.from
// =============================================================================

async function testShareFrom() {
  const source = from('share-from');
  const shared = Share.from(source);
  const consumer = shared.pull();

  const data = await text(consumer);
  assert.strictEqual(data, 'share-from');
}

async function testShareFromRejectsNonStreamable() {
  assert.throws(
    () => Share.from(12345),
    { name: 'TypeError' },
  );
}

// =============================================================================
// Sync share
// =============================================================================

async function testShareSyncBasic() {
  const source = fromSync('sync shared');
  const shared = shareSync(source);

  const consumer = shared.pull();
  const data = textSync(consumer);
  assert.strictEqual(data, 'sync shared');
}

async function testShareSyncMultipleConsumers() {
  function* gen() {
    yield [new TextEncoder().encode('a')];
    yield [new TextEncoder().encode('b')];
    yield [new TextEncoder().encode('c')];
  }

  const shared = shareSync(gen(), { highWaterMark: 16 });

  const c1 = shared.pull();
  const c2 = shared.pull();

  const data1 = textSync(c1);
  const data2 = textSync(c2);

  assert.strictEqual(data1, 'abc');
  assert.strictEqual(data2, 'abc');
}

async function testShareSyncCancel() {
  const source = fromSync('data');
  const shared = shareSync(source);
  const consumer = shared.pull();

  shared.cancel();

  const batches = [];
  for (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// SyncShare.fromSync
// =============================================================================

async function testSyncShareFromSync() {
  const source = fromSync('sync-share-from');
  const shared = SyncShare.fromSync(source);
  const consumer = shared.pull();

  const data = textSync(consumer);
  assert.strictEqual(data, 'sync-share-from');
}

async function testSyncShareFromRejectsNonStreamable() {
  assert.throws(
    () => SyncShare.fromSync(12345),
    { name: 'TypeError' },
  );
}

Promise.all([
  testBasicShare(),
  testShareMultipleConsumers(),
  testShareConsumerCount(),
  testShareCancel(),
  testShareCancelWithReason(),
  testShareAbortSignal(),
  testShareAlreadyAborted(),
  testShareFrom(),
  testShareFromRejectsNonStreamable(),
  testShareSyncBasic(),
  testShareSyncMultipleConsumers(),
  testShareSyncCancel(),
  testSyncShareFromSync(),
  testSyncShareFromRejectsNonStreamable(),
]).then(common.mustCall());
