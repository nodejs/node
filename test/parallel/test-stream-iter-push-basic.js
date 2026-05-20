// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { push, text } = require('stream/iter');

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

async function testWriterEnd() {
  const { writer, readable } = push();

  const totalBytes = writer.endSync();
  assert.strictEqual(totalBytes, 0);

  // Calling endSync again returns byte count (idempotent when closed)
  assert.strictEqual(writer.endSync(), 0);

  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testWriterFail() {
  const { writer, readable } = push();

  writer.fail(new Error('test fail'));

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of readable) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'test fail' },
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
    { name: 'AbortError' },
  );
}

async function testPreAbortedSignal() {
  const ac = new AbortController();
  ac.abort();
  const { readable } = push({ signal: ac.signal });
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) {
      assert.fail('Should not reach here');
    }
  }, { name: 'AbortError' });
}

async function testConsumerBreakWriteSyncReturnsFalse() {
  const { writer, readable } = push({ highWaterMark: 10 });
  writer.writeSync('a');

  // Break after first batch
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) {
    break;
  }

  // After consumer break, writeSync should return false
  assert.strictEqual(writer.writeSync('b'), false);
  assert.strictEqual(writer.desiredSize, null);
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

async function testInvalidBackpressure() {
  assert.throws(() => push({ backpressure: 'banana' }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => push({ backpressure: '' }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  // Valid values should not throw
  for (const bp of ['strict', 'block', 'drop-oldest', 'drop-newest']) {
    push({ backpressure: bp });
  }
}

Promise.all([
  testBasicWriteRead(),
  testMultipleWrites(),
  testDesiredSize(),
  testWriterEnd(),
  testWriterFail(),
  testConsumerBreak(),
  testAbortSignal(),
  testPreAbortedSignal(),
  testConsumerBreakWriteSyncReturnsFalse(),
  testPushWithTransforms(),
  testInvalidBackpressure(),
]).then(common.mustCall());
