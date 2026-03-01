'use strict';

const common = require('../common');
const assert = require('assert');
const { push, text, ondrain } = require('stream/new');

async function testBasicWriteRead() {
  const { writer, readable } = push();

  writer.write('hello');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'hello');
}

async function testMultipleWrites() {
  const { writer, readable } = push({ highWaterMark: 10 });

  writer.write('a');
  writer.write('b');
  writer.write('c');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'abc');
}

async function testDesiredSize() {
  const { writer } = push({ highWaterMark: 3 });

  assert.strictEqual(writer.desiredSize, 3);
  writer.writeSync('a');
  assert.strictEqual(writer.desiredSize, 2);
  writer.writeSync('b');
  assert.strictEqual(writer.desiredSize, 1);
  writer.writeSync('c');
  assert.strictEqual(writer.desiredSize, 0);

  writer.end();
  assert.strictEqual(writer.desiredSize, null);
}

async function testStrictBackpressure() {
  const { writer, readable } = push({
    highWaterMark: 1,
    backpressure: 'strict',
  });

  // First write should succeed synchronously
  assert.strictEqual(writer.writeSync('a'), true);
  // Second write should fail synchronously (buffer full)
  assert.strictEqual(writer.writeSync('b'), false);

  // Consume to free space, then end
  const resultPromise = text(readable);
  writer.end();
  const data = await resultPromise;
  assert.strictEqual(data, 'a');
}

async function testDropOldest() {
  const { writer, readable } = push({
    highWaterMark: 2,
    backpressure: 'drop-oldest',
  });

  writer.writeSync('first');
  writer.writeSync('second');
  // This should drop 'first'
  writer.writeSync('third');
  writer.end();

  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  // Should have 'second' and 'third'
  const allBytes = [];
  for (const batch of batches) {
    for (const chunk of batch) {
      allBytes.push(...chunk);
    }
  }
  const result = new TextDecoder().decode(new Uint8Array(allBytes));
  assert.strictEqual(result, 'secondthird');
}

async function testDropNewest() {
  const { writer, readable } = push({
    highWaterMark: 1,
    backpressure: 'drop-newest',
  });

  writer.writeSync('kept');
  // This is silently dropped
  writer.writeSync('dropped');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'kept');
}

async function testWriterEnd() {
  const { writer, readable } = push();

  const totalBytes = writer.endSync();
  assert.strictEqual(totalBytes, 0);

  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testWriterAbort() {
  const { writer, readable } = push();

  writer.abort(new Error('test abort'));

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of readable) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'test abort' },
  );
}

async function testConsumerBreak() {
  const { writer, readable } = push({ highWaterMark: 10 });

  writer.writeSync('a');
  writer.writeSync('b');
  writer.writeSync('c');

  // Break after first batch
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) {
    break;
  }

  // Writer should now see null desiredSize
  assert.strictEqual(writer.desiredSize, null);
}

async function testAbortSignal() {
  const ac = new AbortController();
  const { readable } = push({ signal: ac.signal });

  ac.abort();

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of readable) {
        assert.fail('Should not reach here');
      }
    },
    (err) => err.name === 'AbortError',
  );
}

async function testOndrain() {
  const { writer } = push({ highWaterMark: 1 });

  // With space available, ondrain resolves immediately
  const drainResult = ondrain(writer);
  assert.ok(drainResult instanceof Promise);
  const result = await drainResult;
  assert.strictEqual(result, true);

  // After close, ondrain returns null
  writer.end();
  assert.strictEqual(ondrain(writer), null);
}

async function testOndainNonDrainable() {
  // Non-drainable objects return null
  assert.strictEqual(ondrain(null), null);
  assert.strictEqual(ondrain({}), null);
  assert.strictEqual(ondrain('string'), null);
}

async function testPushWithTransforms() {
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };

  const { writer, readable } = push(upper);

  writer.write('hello');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'HELLO');
}

Promise.all([
  testBasicWriteRead(),
  testMultipleWrites(),
  testDesiredSize(),
  testStrictBackpressure(),
  testDropOldest(),
  testDropNewest(),
  testWriterEnd(),
  testWriterAbort(),
  testConsumerBreak(),
  testAbortSignal(),
  testOndrain(),
  testOndainNonDrainable(),
  testPushWithTransforms(),
]).then(common.mustCall());
