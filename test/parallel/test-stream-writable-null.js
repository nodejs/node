'use strict';
const common = require('../common');
const assert = require('assert');

const stream = require('stream');

class MyWritable extends stream.Writable {
  constructor(options) {
    super({ autoDestroy: false, ...options });
  }
  _write(chunk, encoding, callback) {
    // eslint-disable-next-line node-core/must-call-assert
    assert.notStrictEqual(chunk, null);
    callback();
  }
}

{
  const m = new MyWritable({ objectMode: true });
  m.on('error', common.mustNotCall());
  assert.throws(() => {
    m.write(null);
  }, {
    code: 'ERR_STREAM_NULL_VALUES'
  });
}

{
  const m = new MyWritable();
  m.on('error', common.mustNotCall());
  assert.throws(() => {
    m.write(false);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}

{ // Should not throw.
  const m = new MyWritable({ objectMode: true });
  m.write(false, assert.ifError);
}

{ // Should not throw.
  const m = new MyWritable({ objectMode: true }).on('error', common.mustNotCall());
  m.write(false, assert.ifError);
}
