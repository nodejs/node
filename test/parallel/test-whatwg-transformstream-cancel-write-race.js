'use strict';

const common = require('../common');
const { TransformStream } = require('stream/web');
const { setTimeout } = require('timers/promises');

// Test for https://github.com/nodejs/node/issues/62036
// A late write racing with reader.cancel() should not throw an internal "transformAlgorithm is not a function" TypeError.

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

  const results = await Promise.allSettled([
    pendingRead,
    pendingCancel,
    pendingLateWrite,
  ]);

  // pendingRead should fulfill (with done:true or a value).
  // pendingCancel should fulfill.
  // pendingLateWrite may reject, but must NOT reject with an internal
  // TypeError about transformAlgorithm not being a function.
  for (const result of results) {
    if (result.status === 'rejected') {
      const err = result.reason;
      if (err instanceof TypeError &&
          /transformAlgorithm is not a function/.test(err.message)) {
        throw new Error(
          'Internal implementation error leaked: ' + err.message
        );
      }
    }
  }
}

test().then(common.mustCall());
