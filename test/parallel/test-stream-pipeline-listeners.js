'use strict';

const common = require('../common');
const { pipeline, Duplex, PassThrough, Writable } = require('stream');
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'no way');
}, 1));

// Ensure that listeners is removed if last stream is readable
// And other stream's listeners unchanged
const a = new PassThrough();
a.end('foobar');
const b = new Duplex({
  write(chunk, encoding, callback) {
    callback();
  }
});
pipeline(a, b, common.mustCall((error) => {
  if (error) {
    assert.ifError(error);
  }

  assert(a.listenerCount('error') > 0);
  assert.strictEqual(b.listenerCount('error'), 0);
  setTimeout(() => {
    assert.strictEqual(b.listenerCount('error'), 0);
    b.destroy(new Error('no way'));
  }, 100);
}));

// If last stream is not readable, will not throw and remove listeners
const e = new PassThrough();
e.end('foobar');
const f = new Writable({
  write(chunk, encoding, callback) {
    callback();
  }
});
pipeline(e, f, common.mustCall((error) => {
  if (error) {
    assert.ifError(error);
  }

  assert(e.listenerCount('error') > 0);
  assert(f.listenerCount('error') > 0);
  setTimeout(() => {
    assert(f.listenerCount('error') > 0);
    f.destroy(new Error('no way'));
  }, 100);
}));
