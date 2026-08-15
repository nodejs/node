'use strict';
const common = require('../common');

// Regression test for https://github.com/nodejs/node/issues/53185
// When a source is piped to multiple destinations and one destination errors
// synchronously in `_write`, the source must keep flowing to the remaining
// healthy destination(s) instead of stalling forever.

const assert = require('assert');
const { PassThrough, Writable, finished } = require('stream');

const source = new PassThrough({ highWaterMark: 16 * 1024 });

// A destination that errors synchronously on its first write.
const failing = new Writable({
  highWaterMark: 16 * 1024,
  write(chunk, encoding, callback) {
    callback(new Error('boom'));
  },
});
failing.on('error', common.mustCall());

// A healthy destination that counts everything it receives.
let received = 0;
const healthy = new Writable({
  highWaterMark: 16 * 1024,
  write(chunk, encoding, callback) {
    received += chunk.length;
    callback();
  },
});

source.pipe(failing);
source.pipe(healthy);

// Two chunks each larger than the highWaterMark so that `write()` returns
// false and the source is paused awaiting a drain.
const chunk = Buffer.alloc(20000, 'a');
source.write(chunk);
source.write(chunk);
source.end();

finished(healthy, common.mustSucceed(() => {
  // Without the fix, the errored destination stays in the source's
  // awaitDrainWriters set, the source never resumes, and `healthy` only
  // receives the first chunk.
  assert.strictEqual(received, chunk.length * 2);
}));
