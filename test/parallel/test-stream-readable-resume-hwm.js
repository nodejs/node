'use strict';
const common = require('../common');
const { Readable } = require('stream');

// readable.resume() should not lead to a ._read() call being scheduled
// when we exceed the high water mark already.

const readable = new Readable({
  read: common.mustNotCall(),
  highWaterMark: 100
});

// Fill up the internal buffer so that we definitely exceed the HWM:
for (let i = 0; i < 10; i++)
  readable.push('a'.repeat(200));

// Call resume, and pause after one chunk.
// The .pause() is just so that we don’t empty the buffer fully, which would
// be a valid reason to call ._read().
readable.resume();
readable.once('data', common.mustCall(() => readable.pause()));
