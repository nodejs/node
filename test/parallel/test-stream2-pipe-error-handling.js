'use strict';
require('../common');
var assert = require('assert');
var stream = require('stream');

{
  let count = 1000;

  const source = new stream.Readable();
  source._read = function(n) {
    n = Math.min(count, n);
    count -= n;
    source.push(Buffer.allocUnsafe(n));
  };

  let unpipedDest;
  source.unpipe = function(dest) {
    unpipedDest = dest;
    stream.Readable.prototype.unpipe.call(this, dest);
  };

  const dest = new stream.Writable();
  dest._write = function(chunk, encoding, cb) {
    cb();
  };

  source.pipe(dest);

  let gotErr = null;
  dest.on('error', function(err) {
    gotErr = err;
  });

  let unpipedSource;
  dest.on('unpipe', function(src) {
    unpipedSource = src;
  });

  const err = new Error('This stream turned into bacon.');
  dest.emit('error', err);
  assert.strictEqual(gotErr, err);
  assert.strictEqual(unpipedSource, source);
  assert.strictEqual(unpipedDest, dest);
}

{
  let count = 1000;

  const source = new stream.Readable();
  source._read = function(n) {
    n = Math.min(count, n);
    count -= n;
    source.push(Buffer.allocUnsafe(n));
  };

  let unpipedDest;
  source.unpipe = function(dest) {
    unpipedDest = dest;
    stream.Readable.prototype.unpipe.call(this, dest);
  };

  const dest = new stream.Writable();
  dest._write = function(chunk, encoding, cb) {
    cb();
  };

  source.pipe(dest);

  let unpipedSource;
  dest.on('unpipe', function(src) {
    unpipedSource = src;
  });

  const err = new Error('This stream turned into bacon.');

  let gotErr = null;
  try {
    dest.emit('error', err);
  } catch (e) {
    gotErr = e;
  }
  assert.strictEqual(gotErr, err);
  assert.strictEqual(unpipedSource, source);
  assert.strictEqual(unpipedDest, dest);
}
