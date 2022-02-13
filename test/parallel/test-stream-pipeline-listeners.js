'use strict';

const common = require('../common');
const { pipeline, Duplex, PassThrough, Writable } = require('stream');
const assert = require('assert');

const a = new PassThrough();
a.end('foobar');

const b = new Duplex({
  write(chunk, encoding, callback) {
    callback();
  }
});

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'no way');
}, 1));

pipeline(a, b, common.mustCall((error) => {
  if (error) {
    assert.ifError(error);
  }

  // Ensure that listeners is removed if last stream is readble
  // And other stream's listeners unchanged
  assert(a.listenerCount('error') > 0);
  assert.strictEqual(b.listenerCount('error'), 0);
  setTimeout(() => {
    assert.strictEqual(b.listenerCount('error'), 0);
    b.destroy(new Error('no way'));
  }, 100);
}));

// If last stream is not readable, will not throw and remove listeners
const c = new PassThrough();
c.end('foobar');
const d = new Writable({
  write(chunk, encoding, callback) {
    callback();
  }
});
pipeline(c, d, common.mustCall((error) => {
  if (error) {
    assert.ifError(error);
  }

  assert(c.listenerCount('error') > 0);
  assert(d.listenerCount('error') > 0);
  setTimeout(() => {
    assert(d.listenerCount('error') > 0);
    d.destroy(new Error('no way'));
  }, 100);
}));
