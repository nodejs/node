'use strict';
require('../common');
const assert = require('assert');
const Duplex = require('stream').Transform;

const stream = new Duplex({ objectMode: true });

assert(stream._readableState.objectMode);
assert(stream._writableState.objectMode);

let written;
let read;

stream._write = function(obj, _, cb) {
  written = obj;
  cb();
};

stream._read = function() {};

stream.on('data', function(obj) {
  read = obj;
});

stream.push({ val: 1 });
stream.end({ val: 2 });

process.on('exit', function() {
  assert.strictEqual(read.val, 1);
  assert.strictEqual(written.val, 2);
});
