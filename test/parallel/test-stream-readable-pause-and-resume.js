'use strict';

const { Readable } = require('stream');
const common = require('../common');

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
  }, 1); // only call ondata once

  rs.on('data', ondata);
}
