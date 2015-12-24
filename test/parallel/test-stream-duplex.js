'use strict';
require('../common');
var assert = require('assert');

var Duplex = require('stream').Transform;

var stream = new Duplex({ objectMode: true });

assert(stream._readableState.objectMode);
assert(stream._writableState.objectMode);

var written;
var read;

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
  assert(read.val === 1);
  assert(written.val === 2);
});
