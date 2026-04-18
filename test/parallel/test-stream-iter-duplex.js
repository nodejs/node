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
  await channelA.close();

  const dataAtB = await text(channelB.readable);
  assert.strictEqual(dataAtB, 'hello from A');
}

async function testBidirectional() {
  const [channelA, channelB] = duplex();

  // A writes to B, B writes to A concurrently
  const writeA = (async () => {
    await channelA.writer.write('A to B');
    await channelA.close();
  })();

  const writeB = (async () => {
    await channelB.writer.write('B to A');
    await channelB.close();
  })();

  const readAtB = text(channelB.readable);
  const readAtA = text(channelA.readable);

  await Promise.all([writeA, writeB]);

  const [dataAtA, dataAtB] = await Promise.all([readAtA, readAtB]);

  assert.strictEqual(dataAtB, 'A to B');
  assert.strictEqual(dataAtA, 'B to A');
}

async function testMultipleWrites() {
  const [channelA, channelB] = duplex({ highWaterMark: 10 });

  await channelA.writer.write('one');
  await channelA.writer.write('two');
  await channelA.writer.write('three');
  await channelA.close();

  const data = await text(channelB.readable);
  assert.strictEqual(data, 'onetwothree');
}

async function testChannelClose() {
  const [channelA, channelB] = duplex();

  await channelA.close();

  // Should be able to close twice without error
  await channelA.close();

  // B's readable should end (A -> B direction is closed)
  const batches = [];
  for await (const batch of channelB.readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testWithOptions() {
  const [channelA, channelB] = duplex({
    highWaterMark: 2,
    backpressure: 'strict',
  });

  await channelA.writer.write('msg');
  await channelA.close();

  const data = await text(channelB.readable);
  assert.strictEqual(data, 'msg');
}

async function testPerChannelOptions() {
  const [channelA, channelB] = duplex({
    a: { highWaterMark: 1 },
    b: { highWaterMark: 4 },
  });

  // Channel A -> B direction uses A's options
  // Channel B -> A direction uses B's options
  await channelA.writer.write('from-a');
  await channelA.close();

  await channelB.writer.write('from-b');
  await channelB.close();

  const [dataAtA, dataAtB] = await Promise.all([
    text(channelA.readable),
    text(channelB.readable),
  ]);

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

async function testEmptyDuplex() {
  const [channelA, channelB] = duplex();

  // Close without writing
  await channelA.close();
  await channelB.close();

  const dataAtA = await bytes(channelA.readable);
  const dataAtB = await bytes(channelB.readable);

  assert.strictEqual(dataAtA.byteLength, 0);
  assert.strictEqual(dataAtB.byteLength, 0);
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
  testEmptyDuplex(),
  testChannelFail(),
  testAbortSignalBothChannels(),
]).then(common.mustCall());
