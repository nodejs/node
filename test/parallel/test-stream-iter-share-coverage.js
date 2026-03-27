// Flags: --experimental-stream-iter
'use strict';

// Coverage tests for share.js: protocol happy path, dispose, throw,
// non-Error source throws.

const common = require('../common');
const assert = require('assert');
const {
  Share,
  SyncShare,
  share,
  shareSync,
  shareProtocol,
  shareSyncProtocol,
  from,
  fromSync,
  text,
  textSync,
} = require('stream/iter');

// Share.from — protocol symbol happy path (returns valid share object)
async function testShareProtocolHappyPath() {
  const obj = {
    [shareProtocol](options) {
      return share(from('protocol-data'), options);
    },
  };
  const shared = Share.from(obj);
  const result = await text(shared.pull());
  assert.strictEqual(result, 'protocol-data');
}

// SyncShare.fromSync — protocol symbol happy path
async function testSyncShareProtocolHappyPath() {
  const obj = {
    [shareSyncProtocol](options) {
      return shareSync(fromSync('sync-protocol'), options);
    },
  };
  const shared = SyncShare.fromSync(obj);
  const result = textSync(shared.pull());
  assert.strictEqual(result, 'sync-protocol');
}

// Async share — Symbol.dispose cancels
async function testShareDispose() {
  const shared = share(from('dispose-test'));
  const consumer = shared.pull();
  shared[Symbol.dispose]();
  assert.strictEqual(shared.consumerCount, 0);
  // Consumer should yield nothing (cancelled before read)
  const result = await text(consumer);
  assert.strictEqual(result, '');
}

// Sync share — Symbol.dispose cancels
async function testSyncShareDispose() {
  const shared = shareSync(fromSync('sync-dispose'));
  const consumer = shared.pull();
  shared[Symbol.dispose]();
  assert.strictEqual(shared.consumerCount, 0);
  const result = textSync(consumer);
  assert.strictEqual(result, '');
}

// Async consumer iterator throw()
async function testAsyncIteratorThrow() {
  const shared = share(from('throw-test'));
  const consumer = shared.pull();
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  const result = await iter.throw(new Error('test-throw'));
  assert.strictEqual(result.done, true);
  assert.strictEqual(shared.consumerCount, 0);
}

// Sync consumer iterator throw()
async function testSyncIteratorThrow() {
  const shared = shareSync(fromSync('throw-sync'));
  const consumer = shared.pull();
  const iter = consumer[Symbol.iterator]();
  const first = iter.next();
  assert.strictEqual(first.done, false);
  const result = iter.throw(new Error('test-throw'));
  assert.strictEqual(result.done, true);
  assert.strictEqual(shared.consumerCount, 0);
}

// Async source throws non-Error value → wrapError
async function testShareSourceThrowsNonError() {
  async function* source() {
    yield [new TextEncoder().encode('ok')];
    throw 'not an error'; // eslint-disable-line no-throw-literal
  }
  const shared = share(source());
  const consumer = shared.pull();
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const batch of consumer) { /* consume */ }
  }, { code: 'ERR_OPERATION_FAILED' });
}

// Sync source throws non-Error value → wrapError
async function testSyncShareSourceThrowsNonError() {
  function* source() {
    yield [new TextEncoder().encode('ok')];
    throw 42; // eslint-disable-line no-throw-literal
  }
  const shared = shareSync(source());
  const consumer = shared.pull();
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const batch of consumer) { /* consume */ }
  }, { code: 'ERR_OPERATION_FAILED' });
}

Promise.all([
  testShareProtocolHappyPath(),
  testSyncShareProtocolHappyPath(),
  testShareDispose(),
  testSyncShareDispose(),
  testAsyncIteratorThrow(),
  testSyncIteratorThrow(),
  testShareSourceThrowsNonError(),
  testSyncShareSourceThrowsNonError(),
]).then(common.mustCall());
