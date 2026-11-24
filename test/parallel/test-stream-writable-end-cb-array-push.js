'use strict';

// Queuing end() callbacks must not depend on a user-modified
// Array.prototype.push.

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

const written = [];
const calls = [];
const writable = new Writable({
  write(chunk, encoding, cb) {
    written[written.length] = chunk.toString();
    setImmediate(cb);
  },
});

writable.on('finish', common.mustCall(() => {
  calls[calls.length] = 'finish';
}));

// Create the callbacks first: common.mustSucceed() itself uses push.
const first = common.mustSucceed(() => {
  calls[calls.length] = 'first';
});
const second = common.mustSucceed(() => {
  calls[calls.length] = 'second';
});

const originalPush = Array.prototype.push;
Array.prototype.push = function() {
  throw new Error('Array.prototype.push should not be called');
};
try {
  // The write is still pending, so both callbacks are queued.
  writable.end('data', first);
  writable.end(second);
} finally {
  Array.prototype.push = originalPush;
}

process.on('exit', () => {
  assert.deepStrictEqual(written, ['data']);
  assert.deepStrictEqual(calls, ['first', 'second', 'finish']);
});
