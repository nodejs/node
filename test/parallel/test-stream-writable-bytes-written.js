'use strict';
const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

{
  const writable = new Writable({
    write: (buf, enc, cb) => {
      cb();
    }
  });
  assert.strictEqual(writable.bytesWritten, 0);
  writable.write('hello');
  assert.strictEqual(writable.bytesWritten, 5);
  writable.end('world');
  assert.strictEqual(writable.bytesWritten, 10);
}

{
  const writable = new Writable({
    objectMode: true,
    write: (buf, enc, cb) => {
      cb();
    }
  });
  assert.strictEqual(writable.bytesWritten, 0);
  writable.write({});
  assert.strictEqual(writable.bytesWritten, 1);
  writable.end({});
  assert.strictEqual(writable.bytesWritten, 2);
}
