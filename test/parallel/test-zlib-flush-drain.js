'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');

const bigData = Buffer.alloc(10240, 'x');

const opts = {
  level: 0,
  highWaterMark: 16
};

const deflater = zlib.createDeflate(opts);

// shim deflater.flush so we can count times executed
var flushCount = 0;
var drainCount = 0;

const flush = deflater.flush;
deflater.flush = function(kind, callback) {
  flushCount++;
  flush.call(this, kind, callback);
};

deflater.write(bigData);

const ws = deflater._writableState;
const beforeFlush = ws.needDrain;
var afterFlush = ws.needDrain;

deflater.flush(function(err) {
  afterFlush = ws.needDrain;
});

deflater.on('drain', function() {
  drainCount++;
});

process.once('exit', function() {
  assert.equal(beforeFlush, true,
               'before calling flush, writable stream should need to drain');
  assert.equal(afterFlush, false,
               'after calling flush, writable stream should not need to drain');
  assert.equal(drainCount, 1,
               'the deflater should have emitted a single drain event');
  assert.equal(flushCount, 2,
               'flush should be called twice');
});
