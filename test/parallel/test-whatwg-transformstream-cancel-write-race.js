'use strict';

const common = require('../common');
const assert = require('assert');
const { TransformStream } = require('stream/web');
const { setTimeout } = require('timers/promises');

// Test for https://github.com/nodejs/node/issues/62036
// A late write racing with reader.cancel() should not throw an
// internal "transformAlgorithm is not a function" TypeError.

async function test() {
  const stream = new TransformStream({
    transform(chunk, controller) {
      controller.enqueue(chunk);
    },
  });

  await setTimeout(0);

  const reader = stream.readable.getReader();
  const writer = stream.writable.getWriter();

  // Release backpressure.
  const pendingRead = reader.read();

  // Simulate client disconnect / shutdown.
  const pendingCancel = reader.cancel(new Error('client disconnected'));

  // Late write racing with cancel.
  const pendingLateWrite = writer.write('late-write');

  const [
    readResult,
    cancelResult,
    lateWriteResult,
  ] = await Promise.allSettled([
    pendingRead,
    pendingCancel,
    pendingLateWrite,
  ]);

  assert.strictEqual(readResult.status, 'fulfilled');
  assert.strictEqual(cancelResult.status, 'fulfilled');
  if (lateWriteResult.status === 'rejected') {
    const err = lateWriteResult.reason;
    const isNotAFunction = err instanceof TypeError &&
        /transformAlgorithm is not a function/.test(err.message);
    assert.ok(!isNotAFunction,
              `Internal implementation error leaked: ${err.message}`);
  }
}

test().then(common.mustCall());
