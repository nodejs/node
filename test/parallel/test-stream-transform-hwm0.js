'use strict';

const common = require('../common');
const assert = require('assert');
const { Transform } = require('stream');

const t = new Transform({
  objectMode: true, highWaterMark: 0,
  transform(chunk, enc, callback) {
    process.nextTick(() => callback(null, chunk, enc));
  }
});

assert.strictEqual(t.write(1), false);
t.on('drain', common.mustCall(() => {
  assert.strictEqual(t.write(2), false);
  t.end();
}));

t.once('readable', common.mustCall(() => {
  assert.strictEqual(t.read(), 1);
  setImmediate(common.mustCall(() => {
    assert.strictEqual(t.read(), null);
    t.once('readable', common.mustCall(() => {
      assert.strictEqual(t.read(), 2);
    }));
  }));
}));
