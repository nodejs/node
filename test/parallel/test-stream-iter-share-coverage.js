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

async function testCompletedSyncConsumerStaysCompleted() {
  const reason = undefined;
  const source = {
    __proto__: null,
    [Symbol.iterator]() {
      return {
        __proto__: null,
        next() { throw reason; },
      };
    },
  };
  const shared = shareSync(source);
  const completed = shared.pull()[Symbol.iterator]();
  const active = shared.pull()[Symbol.iterator]();

  completed.return();
  let caught = false;
  try {
    active.next();
  } catch (error) {
    caught = true;
    assert.strictEqual(error, reason);
  }
  assert.strictEqual(caught, true);
  assert.deepStrictEqual(completed.next(), {
    __proto__: null,
    done: true,
    value: undefined,
  });
}

async function testSyncCancelIgnoresCleanupError() {
  const reason = null;
  const source = {
    __proto__: null,
    [Symbol.iterator]() {
      let done = false;
      return {
        __proto__: null,
        next() {
          if (done) return { done: true, value: undefined };
          done = true;
          return { done: false, value: [Buffer.from('data')] };
        },
        get return() { throw new Error('cleanup failed'); },
      };
    },
  };
  const shared = shareSync(source);
  const iterator = shared.pull()[Symbol.iterator]();

  iterator.next();
  shared.cancel(reason);

  assert.strictEqual(shared.consumerCount, 0);
  assert.throws(() => iterator.next(), (error) => error === reason);
}

async function testAsyncCancelIgnoresCleanupGetterError() {
  const reason = null;
  const source = {
    __proto__: null,
    [Symbol.asyncIterator]() {
      let done = false;
      return {
        __proto__: null,
        next() {
          if (done) return Promise.resolve({ done: true, value: undefined });
          done = true;
          return Promise.resolve({
            done: false,
            value: [Buffer.from('data')],
          });
        },
        get return() { throw new Error('cleanup failed'); },
      };
    },
  };
  const shared = share(source);
  const iterator = shared.pull()[Symbol.asyncIterator]();

  await iterator.next();
  shared.cancel(reason);

  assert.strictEqual(shared.consumerCount, 0);
  await assert.rejects(iterator.next(), (error) => error === reason);
}

// Async source preserves a non-Error thrown value.
async function testShareSourceThrowsNonError() {
  const reason = 'not an error';
  async function* source() {
    yield [new TextEncoder().encode('ok')];
    throw reason;
  }
  const shared = share(source());
  const consumer = shared.pull();
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const batch of consumer) { /* consume */ }
  }, (error) => error === reason);
}

// Sync source preserves a non-Error thrown value.
async function testSyncShareSourceThrowsNonError() {
  const reason = 42;
  function* source() {
    yield [new TextEncoder().encode('ok')];
    throw reason;
  }
  const shared = shareSync(source());
  const consumer = shared.pull();
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const batch of consumer) { /* consume */ }
  }, (error) => error === reason);
}

Promise.all([
  testShareProtocolHappyPath(),
  testSyncShareProtocolHappyPath(),
  testShareDispose(),
  testSyncShareDispose(),
  testAsyncIteratorThrow(),
  testSyncIteratorThrow(),
  testCompletedSyncConsumerStaysCompleted(),
  testSyncCancelIgnoresCleanupError(),
  testAsyncCancelIgnoresCleanupGetterError(),
  testShareSourceThrowsNonError(),
  testSyncShareSourceThrowsNonError(),
]).then(common.mustCall());
