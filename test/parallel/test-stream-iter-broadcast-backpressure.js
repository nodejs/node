// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, text } = require('stream/iter');

// =============================================================================
// Backpressure policies
// =============================================================================

async function testDropOldest() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 2,
    backpressure: 'drop-oldest',
  });
  const consumer = bc.push();

  writer.writeSync('first');
  writer.writeSync('second');
  // This should drop 'first'
  writer.writeSync('third');
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'secondthird');
}

async function testDropNewest() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 1,
    backpressure: 'drop-newest',
  });
  const consumer = bc.push();

  writer.writeSync('kept');
  // This should be silently dropped
  writer.writeSync('dropped');
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'kept');
}

// =============================================================================
// Block backpressure
// =============================================================================

async function testBlockBackpressure() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 1,
    backpressure: 'block',
  });
  const consumer = bc.push();
  writer.writeSync('a');

  // Next write should block
  let writeResolved = false;
  const writePromise = writer.write('b').then(() => { writeResolved = true; });
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
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 1,
    backpressure: 'block',
  });
  const consumer = bc.push();

  writer.writeSync('a');
  const writePromise = writer.write('b');
  await new Promise(setImmediate);

  // Read all and verify content
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  const firstStr = new TextDecoder().decode(first.value[0]);
  assert.strictEqual(firstStr, 'a');

  await writePromise;
  writer.endSync();

  const second = await iter.next();
  assert.strictEqual(second.done, false);
  const secondStr = new TextDecoder().decode(second.value[0]);
  assert.strictEqual(secondStr, 'b');

  const done = await iter.next();
  assert.strictEqual(done.done, true);
}

// Writev async path
async function testWritevAsync() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 10 });
  const consumer = bc.push();

  await writer.writev(['hello', ' ', 'world']);
  await writer.end();

  const data = await text(consumer);
  assert.strictEqual(data, 'hello world');
}

// endSync returns the total byte count
async function testEndSyncReturnValue() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 10 });
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
