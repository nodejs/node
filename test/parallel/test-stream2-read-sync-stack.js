'use strict';
var common = require('../common');
var assert = require('assert');
var Readable = require('stream').Readable;
var r = new Readable();
var N = 256 * 1024;

// Go ahead and allow the pathological case for this test.
// Yes, it's an infinite loop, that's the point.
process.maxTickDepth = N + 2;

var reads = 0;
r._read = function(n) {
  var chunk = reads++ === N ? null : new Buffer(1);
  r.push(chunk);
};

r.on('readable', function onReadable() {
  if (!(r._readableState.length % 256))
    console.error('readable', r._readableState.length);
  r.read(N * 2);
});

var ended = false;
r.on('end', function onEnd() {
  ended = true;
});

r.read(0);

process.on('exit', function() {
  assert(ended);
  console.log('ok');
});
