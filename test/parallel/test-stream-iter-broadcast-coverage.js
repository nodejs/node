// Flags: --experimental-stream-iter
'use strict';

// Coverage tests for broadcast.js: signal abort on pending write,
// sync iterable from, ringbuffer grow.

const common = require('../common');
const assert = require('assert');
const {
  broadcast,
  Broadcast,
  text,
} = require('stream/iter');

// Signal abort on pending write (covers wireBroadcastWriteSignal + removeAt)
async function testBroadcastWriteAbort() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 1,
    backpressure: 'block',
  });
  const consumer = bc.push();

  // Fill the buffer to capacity
  writer.writeSync(new Uint8Array([1]));

  // Next write will block — pass a signal
  const ac = new AbortController();
  const writePromise = writer.write(new Uint8Array([2]),
                                    { signal: ac.signal });

  // Abort the signal
  ac.abort();

  await assert.rejects(writePromise, { name: 'AbortError' });

  // Clean up
  writer.endSync();
  // Drain the consumer
  const result = [];
  for await (const batch of consumer) {
    result.push(...batch);
  }
  assert.ok(result.length >= 1);
}

// Broadcast.from with sync iterable (generator)
async function testBroadcastFromSyncIterable() {
  function* source() {
    yield [new Uint8Array([10, 20])];
    yield [new Uint8Array([30, 40])];
  }

  const { broadcast: bc } = Broadcast.from(source());
  const consumer = bc.push();
  // Just verify it completes without error and produces data
  let count = 0;
  for await (const batch of consumer) {
    count += batch.length;
  }
  assert.ok(count > 0);
}

// Broadcast.from with sync iterable — string chunks
async function testBroadcastFromSyncIterableStrings() {
  function* source() {
    yield 'hello';
    yield ' world';
  }
  const { broadcast: bc } = Broadcast.from(source());
  const consumer = bc.push();
  const result = await text(consumer);
  assert.strictEqual(result, 'hello world');
}

// Ringbuffer grow — push > 16 items without consumer draining
async function testRingbufferGrow() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 32 });
  const consumer = bc.push();

  // Push 20 items (exceeds default ringbuffer capacity of 16)
  for (let i = 0; i < 20; i++) {
    writer.writeSync(new Uint8Array([i]));
  }
  writer.endSync();

  // Read all items back and verify order
  const items = [];
  for await (const batch of consumer) {
    for (const chunk of batch) {
      items.push(chunk[0]);
    }
  }
  assert.strictEqual(items.length, 20);
  for (let i = 0; i < 20; i++) {
    assert.strictEqual(items[i], i);
  }
}

// Broadcast drainableProtocol after close returns null
async function testDrainableAfterClose() {
  const { drainableProtocol } = require('stream/iter');
  const { writer } = broadcast();
  writer.endSync();
  const result = writer[drainableProtocol]();
  // After close, desired should be null
  assert.strictEqual(result, null);
}

Promise.all([
  testBroadcastWriteAbort(),
  testBroadcastFromSyncIterable(),
  testBroadcastFromSyncIterableStrings(),
  testRingbufferGrow(),
  testDrainableAfterClose(),
]).then(common.mustCall());
