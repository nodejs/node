'use strict';

const common = require('../common');
const assert = require('assert');
const {
  ReadableStream,
  TransformStream,
  WritableStream,
} = require('stream/web');

// A late write after the destination has errored returns an already-rejected
// promise. That must not surface as an unhandled rejection when the pipeTo()
// rejection is already handled.
// Refs: https://github.com/nodejs/node/issues/64561

process.on('unhandledRejection', common.mustNotCall());

(async () => {
  const source = new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.enqueue('b');
      controller.close();
    },
  });

  const asyncPassThrough = new TransformStream({
    async transform(chunk, controller) {
      controller.enqueue(chunk);
    },
  });

  const identity = new TransformStream();

  const failingTransform = new TransformStream({
    transform() {
      throw new Error('boom');
    },
  });

  await assert.rejects(
    source
      .pipeThrough(asyncPassThrough)
      .pipeThrough(identity)
      .pipeThrough(failingTransform)
      .pipeTo(new WritableStream()),
    { message: 'boom' },
  );
})().then(common.mustCall());
