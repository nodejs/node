'use strict';
require('../common');
const assert = require('assert');
const stream = require('stream');

const chunk = Buffer.from('hallo');

const util = require('util');

function TestWriter() {
  stream.Writable.call(this);
}
util.inherits(TestWriter, stream.Writable);

TestWriter.prototype._write = function(buffer, encoding, callback) {
  callback(null);
};

const dest = new TestWriter();

// Set this high so that we'd trigger a nextTick warning
// and/or RangeError if we do maybeReadMore wrong.
function TestReader() {
  stream.Readable.call(this, { highWaterMark: 0x10000 });
}
util.inherits(TestReader, stream.Readable);

TestReader.prototype._read = function(size) {
  this.push(chunk);
};

const src = new TestReader();

for (let i = 0; i < 10; i++) {
  src.pipe(dest);
  src.unpipe(dest);
}

assert.equal(src.listeners('end').length, 0);
assert.equal(src.listeners('readable').length, 0);

assert.equal(dest.listeners('unpipe').length, 0);
assert.equal(dest.listeners('drain').length, 0);
assert.equal(dest.listeners('error').length, 0);
assert.equal(dest.listeners('close').length, 0);
assert.equal(dest.listeners('finish').length, 0);

console.error(src._readableState);
process.on('exit', function() {
  src._readableState.buffer.length = 0;
  console.error(src._readableState);
  assert(src._readableState.length >= src._readableState.highWaterMark);
  console.log('ok');
});
