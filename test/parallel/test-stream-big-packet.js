'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const stream = require('stream');

let passed = false;

function PassThrough() {
  stream.Transform.call(this);
}
util.inherits(PassThrough, stream.Transform);
PassThrough.prototype._transform = function(chunk, encoding, done) {
  this.push(chunk);
  done();
};

function TestStream() {
  stream.Transform.call(this);
}
util.inherits(TestStream, stream.Transform);
TestStream.prototype._transform = function(chunk, encoding, done) {
  if (!passed) {
    // Char 'a' only exists in the last write
    passed = chunk.toString().indexOf('a') >= 0;
  }
  done();
};

const s1 = new PassThrough();
const s2 = new PassThrough();
const s3 = new TestStream();
s1.pipe(s3);
// Don't let s2 auto close which may close s3
s2.pipe(s3, {end: false});

// We must write a buffer larger than highWaterMark
const big = Buffer.alloc(s1._writableState.highWaterMark + 1, 'x');

// Since big is larger than highWaterMark, it will be buffered internally.
assert(!s1.write(big));
// 'tiny' is small enough to pass through internal buffer.
assert(s2.write('tiny'));

// Write some small data in next IO loop, which will never be written to s3
// Because 'drain' event is not emitted from s1 and s1 is still paused
setImmediate(s1.write.bind(s1), 'later');

// Assert after two IO loops when all operations have been done.
process.on('exit', function() {
  assert(passed, 'Large buffer is not handled properly by Writable Stream');
});
