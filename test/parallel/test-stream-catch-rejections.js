'use strict';

const common = require('../common');
const stream = require('stream');
const assert = require('assert');

{
  const r = new stream.Readable({
    captureRejections: true,
    read() {
    }
  });
  r.push('hello');
  r.push('world');

  const err = new Error('kaboom');

  r.on('error', common.mustCall((_err) => {
    assert.strictEqual(err, _err);
    assert.strictEqual(r.destroyed, true);
  }));

  r.on('data', async () => {
    throw err;
  });
}

{
  const w = new stream.Writable({
    captureRejections: true,
    highWaterMark: 1,
    write(chunk, enc, cb) {
      process.nextTick(cb);
    }
  });

  const err = new Error('kaboom');

  w.write('hello', () => {
    w.write('world');
  });

  w.on('error', common.mustCall((_err) => {
    assert.strictEqual(err, _err);
    assert.strictEqual(w.destroyed, true);
  }));

  w.on('drain', common.mustCall(async () => {
    throw err;
  }, 2));
}
