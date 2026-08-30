// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { duplex, text, bytes } = require('stream/iter');

// =============================================================================
// Basic duplex
// =============================================================================

async function testBasicDuplex() {
  const [channelA, channelB] = duplex();

  // A writes, B reads
  await channelA.writer.write('hello from A');
  const closing = channelA.close();
  const dataAtB = await text(channelB.readable);
  await closing;
  assert.strictEqual(dataAtB, 'hello from A');
}

async function testBidirectional() {
  const [channelA, channelB] = duplex();

  await channelA.writer.write('A to B');
  await channelB.writer.write('B to A');

  const endA = channelA.writer.end();
  const endB = channelB.writer.end();
  const [dataAtA, dataAtB] = await Promise.all([
    text(channelA.readable),
    text(channelB.readable),
  ]);
  await Promise.all([endA, endB]);
  await Promise.all([channelA.close(), channelB.close()]);

  assert.strictEqual(dataAtB, 'A to B');
  assert.strictEqual(dataAtA, 'B to A');
}

async function testMultipleWrites() {
  const [channelA, channelB] = duplex({ budget: 16384 });

  await channelA.writer.write('one');
  await channelA.writer.write('two');
  await channelA.writer.write('three');
  const closing = channelA.close();
  const data = await text(channelB.readable);
  await closing;
  assert.strictEqual(data, 'onetwothree');
}

async function testChannelClose() {
  const [channelA, channelB] = duplex();
  const iteratorA = channelA.readable[Symbol.asyncIterator]();
  const otherIteratorA = channelA.readable[Symbol.asyncIterator]();
  const pendingRead = iteratorA.next();

  const closing = channelA.close();
  assert.strictEqual(channelA.close(), closing);
  await closing;

  assert.strictEqual((await pendingRead).done, true);
  assert.strictEqual((await otherIteratorA.next()).done, true);
  assert.strictEqual(
    (await channelA.readable[Symbol.asyncIterator]().next()).done, true);
  await assert.rejects(channelB.writer.write('late'), {
    code: 'ERR_INVALID_STATE',
  });

  // B's readable should end (A -> B direction is closed)
  const batches = [];
  for await (const batch of channelB.readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testWithOptions() {
  const [channelA, channelB] = duplex({
    budget: 16384,
    backpressure: 'strict',
  });

  await channelA.writer.write('msg');
  const closing = channelA.close();
  const data = await text(channelB.readable);
  await closing;
  assert.strictEqual(data, 'msg');
}

async function testPerChannelOptions() {
  const [channelA, channelB] = duplex({
    a: { budget: 16384 },
    b: { budget: 16384 },
  });

  // Channel A -> B direction uses A's options
  // Channel B -> A direction uses B's options
  await channelA.writer.write('from-a');
  await channelB.writer.write('from-b');

  const endA = channelA.writer.end();
  const endB = channelB.writer.end();

  const [dataAtA, dataAtB] = await Promise.all([
    text(channelA.readable),
    text(channelB.readable),
  ]);
  await Promise.all([endA, endB]);
  await Promise.all([channelA.close(), channelB.close()]);

  assert.strictEqual(dataAtB, 'from-a');
  assert.strictEqual(dataAtA, 'from-b');
}

async function testAbortSignal() {
  const ac = new AbortController();
  const [channelA] = duplex({ signal: ac.signal });

  ac.abort();

  // Both directions should error
  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of channelA.readable) {
        assert.fail('Should not reach here');
      }
    },
    (err) => err.name === 'AbortError',
  );
}

async function testWriterEndWithPreAbortedSignal() {
  const [channelA, channelB] = duplex();
  const reason = new Error('end aborted');

  await assert.rejects(
    channelA.writer.end({ signal: AbortSignal.abort(reason) }),
    (error) => error === reason,
  );

  await channelA.writer.write('still open');
  const completedEnd = channelA.writer.end();
  assert.strictEqual(await text(channelB.readable), 'still open');
  assert.strictEqual(await completedEnd, 10);
  await channelB.close();
}

async function testEmptyDuplex() {
  const [channelA, channelB] = duplex();

  await channelA.writer.end();
  await channelB.writer.end();

  const dataAtA = await bytes(channelA.readable);
  const dataAtB = await bytes(channelB.readable);
  await Promise.all([channelA.close(), channelB.close()]);

  assert.strictEqual(dataAtA.byteLength, 0);
  assert.strictEqual(dataAtB.byteLength, 0);
}

async function testCloseWaitsForDrain() {
  const [channelA, channelB] = duplex();
  await channelA.writer.write('buffered');

  let closed = false;
  const closing = channelA.close().then(common.mustCall(() => {
    closed = true;
  }));
  await new Promise(setImmediate);
  assert.strictEqual(closed, false);

  assert.strictEqual(await text(channelB.readable), 'buffered');
  await closing;
}

async function testClosePropagatesWriterFailure() {
  const [channelA] = duplex();
  const reason = new Error('writer failed');
  channelA.writer.fail(reason);
  await assert.rejects(channelA.close(), (error) => error === reason);
}

// Channel fail propagation
async function testChannelFail() {
  const [a, b] = duplex();
  a.writer.fail(new Error('channel failed'));
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of b.readable) { /* consume */ }
  }, { message: 'channel failed' });
  await b.close();
}

// Abort signal affects both channels
async function testAbortSignalBothChannels() {
  const ac = new AbortController();
  const [channelA, channelB] = duplex({ signal: ac.signal });

  ac.abort();

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of channelA.readable) {
      assert.fail('Should not reach here');
    }
  }, (err) => err.name === 'AbortError');

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of channelB.readable) {
      assert.fail('Should not reach here');
    }
  }, (err) => err.name === 'AbortError');
}

Promise.all([
  testBasicDuplex(),
  testBidirectional(),
  testMultipleWrites(),
  testChannelClose(),
  testWithOptions(),
  testPerChannelOptions(),
  testAbortSignal(),
  testWriterEndWithPreAbortedSignal(),
  testEmptyDuplex(),
  testCloseWaitsForDrain(),
  testClosePropagatesWriterFailure(),
  testChannelFail(),
  testAbortSignalBothChannels(),
]).then(common.mustCall());
