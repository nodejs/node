'use strict';

const common = require('../common');

// This test ensures that Readable stream will call _read() for streams
// with highWaterMark === 0 upon .read(0) instead of just trying to
// emit 'readable' event.

const assert = require('assert');
const { Readable } = require('stream');

const r = new Readable({
  // Must be called only once upon setting 'readable' listener
  read: common.mustCall(),
  highWaterMark: 0,
});

let pushedNull = false;
// This will trigger read(0) but must only be called after push(null)
// because the we haven't pushed any data
r.on('readable', common.mustCall(() => {
  assert.strictEqual(r.read(), null);
  assert.strictEqual(pushedNull, true);
}));
r.on('end', common.mustCall());
process.nextTick(() => {
  assert.strictEqual(r.read(), null);
  pushedNull = true;
  r.push(null);
});
