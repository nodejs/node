'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

let ticks = 18;
let expectedData = 19;

const rs = new Readable({
  objectMode: true,
  read: () => {
    if (ticks-- > 0)
      return process.nextTick(() => rs.push({}));
    rs.push({});
    rs.push(null);
  }
});

rs.on('end', common.mustCall());
readAndPause();

function readAndPause() {
  // Does a on(data) -> pause -> wait -> resume -> on(data) ... loop.
  // Expects on(data) to never fire if the stream is paused.
  const ondata = common.mustCall((data) => {
    rs.pause();

    expectedData--;
    if (expectedData <= 0)
      return;

    setImmediate(function() {
      rs.removeListener('data', ondata);
      readAndPause();
      rs.resume();
    });
  }, 1); // Only call ondata once

  rs.on('data', ondata);
}

{
  const readable = new Readable({
    read() {}
  });

  function read() {}

  readable.setEncoding('utf8');
  readable.on('readable', read);
  readable.removeListener('readable', read);
  readable.pause();

  process.nextTick(function() {
    assert(readable.isPaused());
  });
}

{
  const { PassThrough } = require('stream');

  const source3 = new PassThrough();
  const target3 = new PassThrough();

  const chunk = Buffer.allocUnsafe(1000);
  while (target3.write(chunk));

  source3.pipe(target3);
  target3.on('drain', common.mustCall(() => {
    assert(!source3.isPaused());
  }));
  target3.on('data', () => {});
}
