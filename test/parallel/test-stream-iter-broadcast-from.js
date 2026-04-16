// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, Broadcast, from, text } = require('stream/iter');

// =============================================================================
// Broadcast.from
// =============================================================================

async function testBroadcastFromAsyncIterable() {
  const source = from('broadcast-from');
  const { broadcast: bc } = Broadcast.from(source);
  const consumer = bc.push();

  const data = await text(consumer);
  assert.strictEqual(data, 'broadcast-from');
}

async function testBroadcastFromNonArrayChunks() {
  // Source that yields single Uint8Array chunks (not arrays)
  const enc = new TextEncoder();
  async function* singleChunkSource() {
    yield enc.encode('hello');
    yield enc.encode(' world');
  }
  const { broadcast: bc } = Broadcast.from(singleChunkSource());
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'hello world');
}

async function testBroadcastFromStringChunks() {
  // Source that yields bare strings (not arrays)
  async function* stringSource() {
    yield 'foo';
    yield 'bar';
  }
  const { broadcast: bc } = Broadcast.from(stringSource());
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'foobar');
}

async function testBroadcastFromMultipleConsumers() {
  const source = from('shared-data');
  const { broadcast: bc } = Broadcast.from(source);

  const c1 = bc.push();
  const c2 = bc.push();

  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);

  assert.strictEqual(data1, 'shared-data');
  assert.strictEqual(data2, 'shared-data');
}

// =============================================================================
// AbortSignal
// =============================================================================

async function testAbortSignal() {
  const ac = new AbortController();
  const { broadcast: bc } = broadcast({ signal: ac.signal });
  const consumer = bc.push();

  ac.abort();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testAlreadyAbortedSignal() {
  const ac = new AbortController();
  ac.abort();

  const { broadcast: bc } = broadcast({ signal: ac.signal });
  const consumer = bc.push();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// Broadcast.from() hang fix - cancel while write blocked on backpressure
// =============================================================================

async function testBroadcastFromCancelWhileBlocked() {
  // Create a slow async source that blocks between yields
  let sourceFinished = false;
  async function* slowSource() {
    const enc = new TextEncoder();
    yield [enc.encode('chunk1')];
    // Simulate a long delay - the cancel should unblock this
    await new Promise((resolve) => setTimeout(resolve, 10000));
    yield [enc.encode('chunk2')];
    sourceFinished = true;
  }

  const { broadcast: bc } = Broadcast.from(slowSource());
  const consumer = bc.push();

  // Read the first chunk
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);

  // Cancel while the source is blocked waiting to yield the next chunk
  bc.cancel();

  // The iteration should complete (not hang)
  const next = await iter.next();
  assert.strictEqual(next.done, true);

  // Source should NOT have finished (we cancelled before chunk2)
  assert.strictEqual(sourceFinished, false);
}

// =============================================================================
// Source error propagation via Broadcast.from()
// =============================================================================

async function testBroadcastFromSourceError() {
  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('broadcast source boom');
  }
  const { broadcast: bc } = Broadcast.from(failingSource());
  const consumer = bc.push();
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of consumer) { /* consume */ }
  }, { message: 'broadcast source boom' });
}

// =============================================================================
// Protocol validation
// =============================================================================

function testBroadcastProtocolReturnsNull() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { return null; },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testBroadcastProtocolReturnsString() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { return 'bad'; },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testBroadcastProtocolReturnsUndefined() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

Promise.all([
  testBroadcastFromAsyncIterable(),
  testBroadcastFromNonArrayChunks(),
  testBroadcastFromStringChunks(),
  testBroadcastFromMultipleConsumers(),
  testAbortSignal(),
  testAlreadyAbortedSignal(),
  testBroadcastFromCancelWhileBlocked(),
  testBroadcastFromSourceError(),
  testBroadcastProtocolReturnsNull(),
  testBroadcastProtocolReturnsString(),
  testBroadcastProtocolReturnsUndefined(),
]).then(common.mustCall());
