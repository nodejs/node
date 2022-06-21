'use strict';
const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');
const { strictEqual } = require('assert');

{
  // Strategy 2
  const streamData = ['a', 'b', 'c', null];

  // Fulfill a Readable object
  const readable = new Readable({
    read: common.mustCall(() => {
      process.nextTick(() => {
        readable.push(streamData.shift());
      });
    }, streamData.length),
  });

  // Use helper to convert it to a Web ReadableStream using ByteLength strategy
  const readableStream = Readable.toWeb(readable, {
    strategy: new ByteLengthQueuingStrategy({ highWaterMark: 1 }),
  });

  assert(!readableStream.locked);
  readableStream.getReader().read().then(common.mustCall());
}

{
  // Strategy 2
  const streamData = ['a', 'b', 'c', null];

  // Fulfill a Readable object
  const readable = new Readable({
    read: common.mustCall(() => {
      process.nextTick(() => {
        readable.push(streamData.shift());
      });
    }, streamData.length),
  });

  // Use helper to convert it to a Web ReadableStream using Count strategy
  const readableStream = Readable.toWeb(readable, {
    strategy: new CountQueuingStrategy({ highWaterMark: 1 }),
  });

  assert(!readableStream.locked);
  readableStream.getReader().read().then(common.mustCall());
}

{
  const desireSizeExpected = 2;

  const stringStream = new ReadableStream(
    {
      start(controller) {
        // Check if the strategy is being assigned on the init of the ReadableStream
        strictEqual(controller.desiredSize, desireSizeExpected);
        controller.enqueue('a');
        controller.enqueue('b');
        controller.close();
      },
    },
    new CountQueuingStrategy({ highWaterMark: desireSizeExpected })
  );

  const reader = stringStream.getReader();

  reader.read().then(common.mustCall());
  reader.read().then(common.mustCall());
  reader.read().then(({ value, done }) => {
    strictEqual(value, undefined);
    strictEqual(done, true);
  });
}
