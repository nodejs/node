'use strict';

require('../common');

const assert = require('node:assert');
const {
  setDefaultHighWaterMark,
  getDefaultHighWaterMark,
  Writable,
  Readable,
  Transform
} = require('stream');

assert.notStrictEqual(getDefaultHighWaterMark(false), 32 * 1000);
setDefaultHighWaterMark(false, 32 * 1000);
assert.strictEqual(getDefaultHighWaterMark(false), 32 * 1000);

assert.notStrictEqual(getDefaultHighWaterMark(true), 32);
setDefaultHighWaterMark(true, 32);
assert.strictEqual(getDefaultHighWaterMark(true), 32);

const w = new Writable({
  write() {}
});
assert.strictEqual(w.writableHighWaterMark, 32 * 1000);

const r = new Readable({
  read() {}
});
assert.strictEqual(r.readableHighWaterMark, 32 * 1000);

const t = new Transform({
  transform() {}
});
assert.strictEqual(t.writableHighWaterMark, 32 * 1000);
assert.strictEqual(t.readableHighWaterMark, 32 * 1000);
