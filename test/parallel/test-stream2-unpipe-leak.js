'use strict';
require('../common');
const assert = require('assert');
const stream = require('stream');

const chunk = Buffer.from('hallo');

class TestWriter extends stream.Writable {
  _write(buffer, encoding, callback) {
    callback(null);
  }
}

const dest = new TestWriter();

// Set this high so that we'd trigger a nextTick warning
// and/or RangeError if we do maybeReadMore wrong.
class TestReader extends stream.Readable {
  constructor() {
    super({
      highWaterMark: 0x10000
    });
  }

  _read(size) {
    this.push(chunk);
  }
}

const src = new TestReader();

for (let i = 0; i < 10; i++) {
  src.pipe(dest);
  src.unpipe(dest);
}

assert.strictEqual(src.listeners('end').length, 0);
assert.strictEqual(src.listeners('readable').length, 0);

assert.strictEqual(dest.listeners('unpipe').length, 0);
assert.strictEqual(dest.listeners('drain').length, 0);
assert.strictEqual(dest.listeners('error').length, 0);
assert.strictEqual(dest.listeners('close').length, 0);
assert.strictEqual(dest.listeners('finish').length, 0);

console.error(src._readableState);
process.on('exit', function() {
  src._readableState.buffer.length = 0;
  console.error(src._readableState);
  assert(src._readableState.length >= src._readableState.highWaterMark);
  console.log('ok');
});
