// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  share,
  Share,
  SyncShare,
  text,
  textSync,

} = require('stream/iter');

// =============================================================================
// Share.from
// =============================================================================

async function testShareFrom() {
  const shared = Share.from(from('share-from'));
  const consumer = shared.pull();

  const data = await text(consumer);
  assert.strictEqual(data, 'share-from');
}

function testShareFromRejectsNonStreamable() {
  assert.throws(
    () => Share.from(12345),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// =============================================================================
// SyncShare.fromSync
// =============================================================================

async function testSyncShareFromSync() {
  const shared = SyncShare.fromSync(fromSync('sync-share-from'));
  const consumer = shared.pull();

  const data = textSync(consumer);
  assert.strictEqual(data, 'sync-share-from');
}

function testSyncShareFromRejectsNonStreamable() {
  assert.throws(
    () => SyncShare.fromSync(12345),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// =============================================================================
// Protocol validation
// =============================================================================

function testShareProtocolReturnsNull() {
  const obj = {
    [Symbol.for('Stream.shareProtocol')]() { return null; },
  };
  assert.throws(
    () => Share.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testShareProtocolReturnsNonObject() {
  const obj = {
    [Symbol.for('Stream.shareProtocol')]() { return 42; },
  };
  assert.throws(
    () => Share.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testSyncShareProtocolReturnsNull() {
  const obj = {
    [Symbol.for('Stream.shareSyncProtocol')]() { return null; },
  };
  assert.throws(
    () => SyncShare.fromSync(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testSyncShareProtocolReturnsNonObject() {
  const obj = {
    [Symbol.for('Stream.shareSyncProtocol')]() { return 'bad'; },
  };
  assert.throws(
    () => SyncShare.fromSync(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

// =============================================================================
// Block backpressure: two consumers, slow consumer blocks the source
// =============================================================================

async function testShareBlockBackpressure() {
  // A source that yields 5 items. With two consumers and highWaterMark: 2,
  // the fast consumer drives the source forward. The slow consumer holds back
  // trimming, causing the buffer to fill. 'block' mode should stall the
  // source pull until the slow consumer catches up.
  const enc = new TextEncoder();
  async function* source() {
    for (let i = 0; i < 5; i++) {
      yield [enc.encode(`item${i}`)];
    }
  }
  const shared = share(source(), { highWaterMark: 2, backpressure: 'block' });
  const fast = shared.pull();
  const slow = shared.pull();

  // Both consumers should ultimately receive all 5 items
  const [fastData, slowData] = await Promise.all([
    text(fast),
    text(slow),
  ]);

  assert.strictEqual(fastData, 'item0item1item2item3item4');
  assert.strictEqual(slowData, 'item0item1item2item3item4');
}

// =============================================================================
// Drop backpressure modes: use a fast + stalled consumer to trigger drops
// =============================================================================

async function testShareDropOldest() {
  // Two consumers, fast reads eagerly then slow reads. With drop-oldest,
  // the slow consumer's cursor is advanced past dropped items, so it
  // misses old data and only sees recent items.
  async function* source() {
    for (let i = 0; i < 4; i++) {
      yield [new TextEncoder().encode(`${i}`)];
    }
  }
  const shared = share(source(), { highWaterMark: 2, backpressure: 'drop-oldest' });
  const fast = shared.pull();
  const slow = shared.pull();

  // Fast consumer reads all items
  const fastItems = [];
  for await (const batch of fast) {
    for (const chunk of batch) {
      fastItems.push(new TextDecoder().decode(chunk));
    }
  }
  assert.strictEqual(fastItems.length, 4);

  // Slow consumer reads after fast is done — old items were dropped
  const slowItems = [];
  for await (const batch of slow) {
    for (const chunk of batch) {
      slowItems.push(new TextDecoder().decode(chunk));
    }
  }
  // The slow consumer should see fewer items than were produced
  assert.ok(slowItems.length < 4,
            `Expected < 4 items after drop-oldest, got ${slowItems.length}`);
  assert.ok(slowItems.length > 0,
            'Expected at least some items after drop-oldest');
  // The last item should always be present (most recent items kept)
  assert.strictEqual(slowItems[slowItems.length - 1], '3');
}

async function testShareDropNewest() {
  // With drop-newest and a stalled consumer, the async path allows the
  // buffer to grow beyond highWaterMark (the "drop" applies to the
  // backpressure signal, not the buffer contents). Both consumers
  // ultimately see all items.
  async function* source() {
    for (let i = 0; i < 4; i++) {
      yield [new TextEncoder().encode(`${i}`)];
    }
  }
  const shared = share(source(), { highWaterMark: 2, backpressure: 'drop-newest' });
  const fast = shared.pull();
  const slow = shared.pull();

  // Fast consumer reads all items
  const fastItems = [];
  for await (const batch of fast) {
    for (const chunk of batch) {
      fastItems.push(new TextDecoder().decode(chunk));
    }
  }
  assert.strictEqual(fastItems.length, 4);

  // Slow consumer also sees all items (buffer grew past hwm)
  const slowItems = [];
  for await (const batch of slow) {
    for (const chunk of batch) {
      slowItems.push(new TextDecoder().decode(chunk));
    }
  }
  assert.strictEqual(slowItems.length, 4);
  assert.strictEqual(slowItems[0], '0');
  assert.strictEqual(slowItems[3], '3');
}

// =============================================================================
// Strict backpressure: should throw when buffer overflows
// =============================================================================

async function testShareStrictBackpressure() {
  async function* source() {
    for (let i = 0; i < 10; i++) {
      yield [new TextEncoder().encode(`${i}`)];
    }
  }
  const shared = share(source(), { highWaterMark: 2, backpressure: 'strict' });
  const fast = shared.pull();
  // Create a second consumer that never reads — this prevents buffer trimming
  shared.pull();

  // The fast consumer's pulls will eventually cause the buffer to exceed
  // the highWaterMark (since the slow consumer prevents trimming),
  // triggering an ERR_OUT_OF_RANGE error.
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of fast) { /* consume */ }
  }, { code: 'ERR_OUT_OF_RANGE' });
}

Promise.all([
  testShareFrom(),
  testShareFromRejectsNonStreamable(),
  testSyncShareFromSync(),
  testSyncShareFromRejectsNonStreamable(),
  testShareProtocolReturnsNull(),
  testShareProtocolReturnsNonObject(),
  testSyncShareProtocolReturnsNull(),
  testSyncShareProtocolReturnsNonObject(),
  testShareBlockBackpressure(),
  testShareDropOldest(),
  testShareDropNewest(),
  testShareStrictBackpressure(),
]).then(common.mustCall());
