'use strict';
const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

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
