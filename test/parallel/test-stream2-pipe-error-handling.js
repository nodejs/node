'use strict';
var common = require('../common');
var assert = require('assert');
var stream = require('stream');

(function testErrorListenerCatches() {
  var count = 1000;

  var source = new stream.Readable();
  source._read = function(n) {
    n = Math.min(count, n);
    count -= n;
    source.push(new Buffer(n));
  };

  var unpipedDest;
  source.unpipe = function(dest) {
    unpipedDest = dest;
    stream.Readable.prototype.unpipe.call(this, dest);
  };

  var dest = new stream.Writable();
  dest._write = function(chunk, encoding, cb) {
    cb();
  };

  source.pipe(dest);

  var gotErr = null;
  dest.on('error', function(err) {
    gotErr = err;
  });

  var unpipedSource;
  dest.on('unpipe', function(src) {
    unpipedSource = src;
  });

  var err = new Error('This stream turned into bacon.');
  dest.emit('error', err);
  assert.strictEqual(gotErr, err);
  assert.strictEqual(unpipedSource, source);
  assert.strictEqual(unpipedDest, dest);
})();

(function testErrorWithoutListenerThrows() {
  var count = 1000;

  var source = new stream.Readable();
  source._read = function(n) {
    n = Math.min(count, n);
    count -= n;
    source.push(new Buffer(n));
  };

  var unpipedDest;
  source.unpipe = function(dest) {
    unpipedDest = dest;
    stream.Readable.prototype.unpipe.call(this, dest);
  };

  var dest = new stream.Writable();
  dest._write = function(chunk, encoding, cb) {
    cb();
  };

  source.pipe(dest);

  var unpipedSource;
  dest.on('unpipe', function(src) {
    unpipedSource = src;
  });

  var err = new Error('This stream turned into bacon.');

  var gotErr = null;
  try {
    dest.emit('error', err);
  } catch (e) {
    gotErr = e;
  }
  assert.strictEqual(gotErr, err);
  assert.strictEqual(unpipedSource, source);
  assert.strictEqual(unpipedDest, dest);
})();
