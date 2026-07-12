'use strict';

const common = require('../common');
const assert = require('assert');

// Regression test for the ring-buffer [[queue]] backing store used by the
// WHATWG stream controllers: exercises growth, wrap-around, drain-rewind
// and shrink behavior through the public API only.

async function testDeepDefaultQueue() {
  // Queue 3000 chunks before reading anything: the backing ring must grow
  // (and re-linearize) repeatedly, then fully drain (shrink on rewind).
  const rs = new ReadableStream({
    start(controller) {
      for (let i = 0; i < 3000; i++)
        controller.enqueue(i);
      controller.close();
    },
  }, { highWaterMark: Infinity });

  const reader = rs.getReader();
  for (let i = 0; i < 3000; i++) {
    const { value, done } = await reader.read();
    assert.strictEqual(done, false);
    assert.strictEqual(value, i);
  }
  const { done } = await reader.read();
  assert.strictEqual(done, true);
}

async function testInterleavedWraparound() {
  // Interleave enqueues and reads so head/tail wrap around the ring many
  // times without ever growing it, including repeated empty transitions.
  let enqueued = 0;
  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    },
  });

  const reader = rs.getReader();
  let expected = 0;
  for (let round = 0; round < 500; round++) {
    const burst = (round % 3) + 1;
    for (let i = 0; i < burst; i++)
      controller.enqueue(enqueued++);
    for (let i = 0; i < burst; i++) {
      const { value, done } = await reader.read();
      assert.strictEqual(done, false);
      assert.strictEqual(value, expected++);
    }
  }
  assert.strictEqual(expected, enqueued);
}

async function testCustomSizesSurviveQueueing() {
  // Fractional and zero sizes must round-trip through the queue exactly
  // (desiredSize accounting depends on the stored per-chunk size).
  const sizes = [0.25, 0, 1.75, 3, 0.5];
  const chunks = ['a', 'b', 'c', 'd', 'e'];
  let controller;
  const rs = new ReadableStream({
    start(c) {
      controller = c;
    },
  }, {
    highWaterMark: 10,
    size(chunk) {
      return sizes[chunks.indexOf(chunk)];
    },
  });

  let expectedDesired = 10;
  for (let i = 0; i < chunks.length; i++) {
    controller.enqueue(chunks[i]);
    expectedDesired -= sizes[i];
    assert.strictEqual(controller.desiredSize, expectedDesired);
  }
  const reader = rs.getReader();
  for (let i = 0; i < chunks.length; i++) {
    const { value } = await reader.read();
    assert.strictEqual(value, chunks[i]);
    expectedDesired += sizes[i];
    assert.strictEqual(controller.desiredSize, expectedDesired);
  }
}

async function testByteQueueUnevenReads() {
  // Enqueue 7-byte chunks but read through 4-byte BYOB views: the byte
  // queue's head record is partially consumed in place across reads.
  const total = 7 * 300;
  let written = 0;
  const rs = new ReadableStream({
    type: 'bytes',
    pull(controller) {
      if (written >= total) {
        controller.close();
        return;
      }
      const chunk = new Uint8Array(7);
      for (let i = 0; i < 7; i++)
        chunk[i] = (written + i) % 251;
      written += 7;
      controller.enqueue(chunk);
    },
  });

  const reader = rs.getReader({ mode: 'byob' });
  let read = 0;
  while (read < total) {
    const { value, done } = await reader.read(new Uint8Array(4));
    if (done) break;
    for (let i = 0; i < value.length; i++) {
      assert.strictEqual(value[i], (read + i) % 251,
                         `byte ${read + i} mismatch`);
    }
    read += value.length;
  }
  assert.strictEqual(read, total);
}

async function testDeepByteQueueBurst() {
  // Fill the byte queue with 2500 records before the default reader
  // drains it, forcing ring growth and the drain-to-empty rewind.
  const rs = new ReadableStream({
    type: 'bytes',
    start(controller) {
      for (let i = 0; i < 2500; i++)
        controller.enqueue(new Uint8Array([i & 0xff]));
      controller.close();
    },
  });

  const reader = rs.getReader();
  for (let i = 0; i < 2500; i++) {
    const { value, done } = await reader.read();
    assert.strictEqual(done, false);
    assert.strictEqual(value[0], i & 0xff);
  }
  const { done } = await reader.read();
  assert.strictEqual(done, true);
}

async function testWritableQueueBackpressureDrain() {
  // Buffer thousands of writes behind a slow sink, then let it drain:
  // the writable controller queue grows, wraps, and empties while every
  // chunk is delivered in order.
  const received = [];
  let release;
  const gate = new Promise((resolve) => { release = resolve; });
  const ws = new WritableStream({
    async write(chunk) {
      if (received.length === 0) await gate;
      received.push(chunk);
    },
  }, { highWaterMark: 1 });

  const writer = ws.getWriter();
  const writes = [];
  for (let i = 0; i < 2000; i++)
    writes.push(writer.write(i));
  release();
  await writer.close();
  await Promise.all(writes);
  assert.strictEqual(received.length, 2000);
  for (let i = 0; i < 2000; i++)
    assert.strictEqual(received[i], i);
}

(async () => {
  await testDeepDefaultQueue();
  await testInterleavedWraparound();
  await testCustomSizesSurviveQueueing();
  await testByteQueueUnevenReads();
  await testDeepByteQueueBurst();
  await testWritableQueueBackpressureDrain();
})().then(common.mustCall());
