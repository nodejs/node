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
