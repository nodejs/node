'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

const writable = new stream.Writable();

writable._writev = common.mustCall((chunks, cb) => {
  assert.strictEqual(chunks.length, 2);
  cb();
}, 1);

writable._write = common.mustCall((chunk, encoding, cb) => {
  cb();
}, 1);

// first cork
writable.cork();
assert.strictEqual(writable._writableState.corked, 1);
assert.strictEqual(writable._writableState.bufferedRequestCount, 0);

// cork again
writable.cork();
assert.strictEqual(writable._writableState.corked, 2);

// the first chunk is buffered
writable.write('first chunk');
assert.strictEqual(writable._writableState.bufferedRequestCount, 1);

// first uncork does nothing
writable.uncork();
assert.strictEqual(writable._writableState.corked, 1);
assert.strictEqual(writable._writableState.bufferedRequestCount, 1);

process.nextTick(uncork);

// The second chunk is buffered, because we uncork at the end of tick
writable.write('second chunk');
assert.strictEqual(writable._writableState.corked, 1);
assert.strictEqual(writable._writableState.bufferedRequestCount, 2);

function uncork() {
  // second uncork flushes the buffer
  writable.uncork();
  assert.strictEqual(writable._writableState.corked, 0);
  assert.strictEqual(writable._writableState.bufferedRequestCount, 0);

  // verify that end() uncorks correctly
  writable.cork();
  writable.write('third chunk');
  writable.end();

  // end causes an uncork() as well
  assert.strictEqual(writable._writableState.corked, 0);
  assert.strictEqual(writable._writableState.bufferedRequestCount, 0);
}
