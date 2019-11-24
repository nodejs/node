'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');

class MyWritable extends stream.Writable {
  constructor(options) {
    super({ autoDestroy: false, ...options });
  }
  _write(chunk, encoding, callback) {
    assert.notStrictEqual(chunk, null);
    callback();
  }
}

assert.throws(
  () => {
    const m = new MyWritable({ objectMode: true });
    m.write(null, (err) => assert.ok(err));
  },
  {
    code: 'ERR_STREAM_NULL_VALUES',
    name: 'TypeError',
    message: 'May not write null values to stream'
  }
);

{ // Should not throw.
  const m = new MyWritable({ objectMode: true }).on('error', assert);
  m.write(null, assert);
}

assert.throws(
  () => {
    const m = new MyWritable();
    m.write(false, (err) => assert.ok(err));
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  }
);

{ // Should not throw.
  const m = new MyWritable().on('error', assert);
  m.write(false, assert);
}

{ // Should not throw.
  const m = new MyWritable({ objectMode: true });
  m.write(false, assert.ifError);
}

{ // Should not throw.
  const m = new MyWritable({ objectMode: true }).on('error', (e) => {
    assert.ifError(e || new Error('should not get here'));
  });
  m.write(false, assert.ifError);
}
