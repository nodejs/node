'use strict';

const common = require('../common');

// This test ensures that Readable stream will continue to call _read
// for streams with highWaterMark === 0 once the stream returns data
// by calling push() asynchronously.

const { Readable } = require('stream');

let count = 5;

const r = new Readable({
  // Called 6 times: First 5 return data, last one signals end of stream.
  read: common.mustCall(() => {
    process.nextTick(common.mustCall(() => {
      if (count--)
        r.push('a');
      else
        r.push(null);
    }));
  }, 6),
  highWaterMark: 0,
});

r.on('end', common.mustCall());
r.on('data', common.mustCall(5));
