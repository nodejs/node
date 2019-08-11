'use strict';

const common = require('../common');
const stream = require('stream');
const assert = require('assert');

{
  const transform = new stream.Transform({
    transform: _transform,
    highWaterMark: 1
  });

  function _transform(chunk, encoding, cb) {
    assert.strictEqual(transform._writableState.needDrain, false);
    cb();
  }

  assert.strictEqual(transform._writableState.needDrain, false);

  transform.write('asdasd', common.mustCall(() => {
    assert.strictEqual(transform._writableState.needDrain, false);
  }));

  assert.strictEqual(transform._writableState.needDrain, false);
}

{
  const w = new stream.Writable({
    highWaterMark: 1
  });

  w._write = (chunk, encoding, cb) => {
    cb();
  };
  w.on('drain', common.mustNotCall());

  assert.strictEqual(w.write('asd'), true);
}


{
  const w = new stream.Writable({
    highWaterMark: 1
  });

  w._write = (chunk, encoding, cb) => {
    process.nextTick(cb);
  };
  w.on('drain', common.mustCall());

  assert.strictEqual(w.write('asd'), false);
}
