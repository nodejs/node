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

  source.pipeTo(dest, { preventCancel: true }).then(
    common.mustNotCall('pipeTo should not resolve'),
    common.mustCall((err) => {
      assert.strictEqual(err.message, 'destination errored');
    })
  );

  setImmediate(common.mustCall(() => {
    destController.error(new Error('destination errored'));
    sourceController.enqueue('chunk');
  }));
}
