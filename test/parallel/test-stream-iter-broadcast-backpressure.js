// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, text } = require('stream/iter');

// =============================================================================
// Backpressure policies
// =============================================================================

async function testDropOldest() {
  const chunk1 = new Uint8Array(16384).fill(49); // '1'
  const chunk2 = new Uint8Array(16384).fill(50); // '2'
  const chunk3 = new Uint8Array(16384).fill(51); // '3'
  const { writer, broadcast: bc } = broadcast({
    budget: 32768,
    backpressure: 'drop-oldest',
  });
  const consumer = bc.push();

  writer.writeSync(chunk1); // 16384 < 32768
  writer.writeSync(chunk2); // 32768 >= 32768
  // Buffer full: this drops chunk1, adds chunk3
  writer.writeSync(chunk3);
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, '2'.repeat(16384) + '3'.repeat(16384));
}

async function testDropNewest() {
  const kept = new Uint8Array(16384).fill(75);    // 'K'
  const dropped = new Uint8Array(16384).fill(68); // 'D'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'drop-newest',
  });
  const consumer = bc.push();

  writer.writeSync(kept);
  // Buffer full: new write is silently discarded
  writer.writeSync(dropped);
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'K'.repeat(16384));
}

// =============================================================================
// Block backpressure
// =============================================================================

async function testBlockBackpressure() {
  const kChunk = new Uint8Array(16384);
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const consumer = bc.push();
  writer.writeSync(kChunk);

  // Next write should block
  let writeResolved = false;
  const writePromise = writer.write(kChunk).then(() => { writeResolved = true; });
  await new Promise(setImmediate);
  assert.strictEqual(writeResolved, false);

  // Drain consumer to unblock the pending write
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  await new Promise(setImmediate);
  assert.strictEqual(writeResolved, true);

  writer.endSync();
  // Drain remaining data and verify completion
  const second = await iter.next();
  assert.strictEqual(second.done, false);
  await writePromise;
}

// Verify block backpressure data flows correctly end-to-end
async function testBlockBackpressureContent() {
  const chunk1 = new Uint8Array(16384).fill(65); // 'A'
  const chunk2 = new Uint8Array(16384).fill(66); // 'B'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const consumer = bc.push();

  writer.writeSync(chunk1);
  const writePromise = writer.write(chunk2);
  await new Promise(setImmediate);

  // Read all and verify content
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  assert.strictEqual(first.value[0].byteLength, 16384);
  assert.strictEqual(first.value[0][0], 65); // 'A'

  await writePromise;
  writer.endSync();

  const second = await iter.next();
  assert.strictEqual(second.done, false);
  assert.strictEqual(second.value[0].byteLength, 16384);
  assert.strictEqual(second.value[0][0], 66); // 'B'

  const done = await iter.next();
  assert.strictEqual(done.done, true);
}

// Writev async path
async function testWritevAsync() {
  const { writer, broadcast: bc } = broadcast({ budget: 16384 });
  const consumer = bc.push();

  await writer.writev(['hello', ' ', 'world']);
  await writer.end();

  const data = await text(consumer);
  assert.strictEqual(data, 'hello world');
}

// endSync returns the total byte count
async function testEndSyncReturnValue() {
  const { writer, broadcast: bc } = broadcast({ budget: 16384 });
  bc.push(); // Need a consumer to write to

  writer.writeSync('hello'); // 5 bytes
  writer.writeSync(' world'); // 6 bytes
  const total = writer.endSync();
  assert.strictEqual(total, 11);
}

Promise.all([
  testDropOldest(),
  testDropNewest(),
  testBlockBackpressure(),
  testBlockBackpressureContent(),
  testWritevAsync(),
  testEndSyncReturnValue(),
]).then(common.mustCall());
