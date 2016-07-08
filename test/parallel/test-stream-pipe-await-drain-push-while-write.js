'use strict';
const common = require('../common');
const stream = require('stream');

// A writable stream which pushes data onto the stream which pipes into it,
// but only the first time it's written to. Since it's not paused at this time,
// a second write will occur. If the pipe increases awaitDrain twice, we'll
// never get subsequent chunks because 'drain' is only emitted once.
const writable = new stream.Writable({
  write: common.mustCall((chunk, encoding, cb) => {
    if (chunk.length === 32 * 1024) { // first chunk
      readable.push(new Buffer(33 * 1024)); // above hwm
    }
    cb();
  }, 3)
});

// A readable stream which produces two buffers.
const bufs = [new Buffer(32 * 1024), new Buffer(33 * 1024)]; // above hwm
const readable = new stream.Readable({
  read: function() {
    while (bufs.length > 0) {
      this.push(bufs.shift());
    }
  }
});

readable.pipe(writable);
