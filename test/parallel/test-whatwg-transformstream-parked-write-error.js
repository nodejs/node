'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const { TransformStream } = require('stream/web');
const { setImmediate } = require('timers/promises');

// A transform sink write arriving while the stream has backpressure is
// parked until backpressure clears. These cases complete a parked write
// while the writable side is already erroring.
//
// The setImmediate() lets the start algorithm settle so the write below
// reaches the sink and parks (backpressure is set until the readable
// side pulls).

test('readable.cancel() rejects a parked write with the cancel reason', async () => {
  const stream = new TransformStream();
  const writer = stream.writable.getWriter();
  await setImmediate();

  const reason = new Error('cancelled');
  const write = writer.write('parked');
  await stream.readable.cancel(reason);
  await assert.rejects(write, (err) => err === reason);
});

test('controller.error() rejects a parked write with the stored error', async () => {
  let controller;
  const stream = new TransformStream({
    start(c) { controller = c; },
  });
  const writer = stream.writable.getWriter();
  await setImmediate();

  const reason = new Error('boom');
  const write = writer.write('parked');
  controller.error(reason);
  await assert.rejects(write, (err) => err === reason);
  await assert.rejects(writer.closed, (err) => err === reason);
});

test('controller.terminate() rejects a parked write', async () => {
  let controller;
  const stream = new TransformStream({
    start(c) { controller = c; },
  });
  const writer = stream.writable.getWriter();
  await setImmediate();

  const write = writer.write('parked');
  controller.terminate();
  await assert.rejects(write, {
    name: 'TypeError',
    message: /terminated/,
  });
});
