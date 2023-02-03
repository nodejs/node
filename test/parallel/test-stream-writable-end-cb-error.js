'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

{
  // Invoke end callback on failure.
  const writable = new stream.Writable();

  const _err = new Error('kaboom');
  writable._write = (chunk, encoding, cb) => {
    process.nextTick(cb, _err);
  };

  writable.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
  }));
  writable.write('asd');
  writable.end(common.mustCall((err) => {
    assert.strictEqual(err, _err);
  }));
  writable.end(common.mustCall((err) => {
    assert.strictEqual(err, _err);
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
    assert.strictEqual(err, null);
  }));

  writable.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'kaboom');
  }));
  writable.on('finish', common.mustCall(() => {
    assert.strictEqual(called, true);
    writable.emit('error', new Error('kaboom'));
  }));
}

{
  const w = new stream.Writable({
    write(chunk, encoding, callback) {
      setImmediate(callback);
    },
    finish(callback) {
      setImmediate(callback);
    }
  });
  w.end('testing ended state', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  assert.strictEqual(w.destroyed, false);
  assert.strictEqual(w.writableEnded, true);
  w.end(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  assert.strictEqual(w.destroyed, false);
  assert.strictEqual(w.writableEnded, true);
  w.end('end', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  assert.strictEqual(w.destroyed, true);
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  w.on('finish', common.mustNotCall());
}
