// Flags: --expose-brotli
'use strict';
require('../common');
const assert = require('assert');
const brotli = require('brotli');

const bigData = Buffer.alloc(10240, 'x');

const opts = {
  quality: 0,
  highWaterMark: 16
};

const deflater = new brotli.Compress(opts);

// shim deflater.flush so we can count times executed
let flushCount = 0;
let drainCount = 0;

const flush = deflater.flush;
deflater.flush = function(callback, oldcb) {
  flushCount++;
  flush.call(this, callback);
};

deflater.on('data', function() {});

deflater.write(bigData);

const ws = deflater._writableState;
const beforeFlush = ws.needDrain;
let afterFlush = ws.needDrain;

deflater.flush(function(err) {
  afterFlush = ws.needDrain;
});

deflater.on('drain', function() {
  drainCount++;
});

process.once('exit', function() {
  assert.strictEqual(
    beforeFlush, true);
  assert.strictEqual(
    afterFlush, false);
  assert.strictEqual(
    drainCount, 1);
  assert.strictEqual(
    flushCount, 2);
});
