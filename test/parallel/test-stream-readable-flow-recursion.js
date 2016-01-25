'use strict';
require('../common');
var assert = require('assert');

// this test verifies that passing a huge number to read(size)
// will push up the highWaterMark, and cause the stream to read
// more data continuously, but without triggering a nextTick
// warning or RangeError.

var Readable = require('stream').Readable;

// throw an error if we trigger a nextTick warning.
process.throwDeprecation = true;

var stream = new Readable({ highWaterMark: 2 });
var reads = 0;
var total = 5000;
stream._read = function(size) {
  reads++;
  size = Math.min(size, total);
  total -= size;
  if (size === 0)
    stream.push(null);
  else
    stream.push(Buffer.allocUnsafe(size));
};

var depth = 0;

function flow(stream, size, callback) {
  depth += 1;
  var chunk = stream.read(size);

  if (!chunk)
    stream.once('readable', flow.bind(null, stream, size, callback));
  else
    callback(chunk);

  depth -= 1;
  console.log('flow(' + depth + '): exit');
}

flow(stream, 5000, function() {
  console.log('complete (' + depth + ')');
});

process.on('exit', function(code) {
  assert.equal(reads, 2);
  // we pushed up the high water mark
  assert.equal(stream._readableState.highWaterMark, 8192);
  // length is 0 right now, because we pulled it all out.
  assert.equal(stream._readableState.length, 0);
  assert(!code);
  assert.equal(depth, 0);
  console.log('ok');
});
