'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

{
  // Invoke end callback on failure.
  const writable = new stream.Writable();

  writable._write = (chunk, encoding, cb) => {
    process.nextTick(cb, new Error('kaboom'));
  };

  writable.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'kaboom');
  }));
  writable.write('asd');
  writable.end(common.mustCall((err) => {
    assert.strictEqual(err.message, 'kaboom');
  }));
  writable.end(common.mustCall((err) => {
    assert.strictEqual(err.message, 'kaboom');
  }));
}

{
  // Don't invoke end callback twice
  const writable = new stream.Writable();

  writable._write = (chunk, encoding, cb) => {
    process.nextTick(cb);
  };

  let called = false;
  writable.end('asd', common.mustCall((err) => {
    called = true;
    assert.strictEqual(err, undefined);
  }));

  writable.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'kaboom');
  }));
  writable.on('finish', common.mustCall(() => {
    assert.strictEqual(called, true);
    writable.emit('error', new Error('kaboom'));
  }));
}
