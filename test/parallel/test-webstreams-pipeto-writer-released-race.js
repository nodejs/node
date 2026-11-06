'use strict';

const common = require('../common');
const assert = require('assert');
const { ReadableStream, WritableStream } = require('stream/web');

{
  let sourceController;
  let destController;

  const source = new ReadableStream({
    start(controller) {
      sourceController = controller;
    },
  });

  const dest = new WritableStream({
    start(controller) {
      destController = controller;
    },
    write() {},
  });

  assert.rejects(
    source.pipeTo(dest, { preventCancel: true }),
    { message: 'destination errored' },
  ).then(common.mustCall());

  setImmediate(common.mustCall(() => {
    destController.error(new Error('destination errored'));
    sourceController.enqueue('chunk');
  }));
}

{
  const ac = new AbortController();
  let sourceController;
  const chunks = [];

  const source = new ReadableStream({
    start(controller) {
      sourceController = controller;
    },
  }, { highWaterMark: 0 });

  const dest = new WritableStream({
    write: common.mustCall((chunk) => {
      chunks.push(chunk);
    }),
  }, { highWaterMark: 1 });

  assert.rejects(
    source.pipeTo(dest, { signal: ac.signal }),
    { name: 'AbortError' },
  ).then(common.mustCall(() => {
    assert.deepStrictEqual(chunks, ['chunk']);
  }));

  setImmediate(common.mustCall(() => {
    sourceController.enqueue('chunk');
    ac.abort();
  }));
}
