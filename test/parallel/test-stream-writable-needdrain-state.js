'use strict';

const common = require('../common');
const stream = require('stream');
const assert = require('assert');

const transform = new stream.Transform({
  transform: _transform,
  highWaterMark: 1
});

function _transform(chunk, encoding, cb) {
  process.nextTick(() => {
    assert.strictEqual(transform._writableState.needDrain, true);
    cb();
  });
}

assert.strictEqual(transform._writableState.needDrain, false);

transform.write('asdasd', common.mustCall(() => {
  assert.strictEqual(transform._writableState.needDrain, false);
}));

assert.strictEqual(transform._writableState.needDrain, true);
