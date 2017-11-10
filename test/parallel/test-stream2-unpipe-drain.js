'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');

class TestWriter extends stream.Writable {
  _write(buffer, encoding, callback) {
    console.log('write called');
    // super slow write stream (callback never called)
  }
}

const dest = new TestWriter();

class TestReader extends stream.Readable {
  constructor() {
    super();
    this.reads = 0;
  }

  _read(size) {
    this.reads += 1;
    this.push(Buffer.alloc(size));
  }
}

const src1 = new TestReader();
const src2 = new TestReader();

src1.pipe(dest);

src1.once('readable', function() {
  process.nextTick(function() {

    src2.pipe(dest);

    src2.once('readable', function() {
      process.nextTick(function() {

        src1.unpipe(dest);
      });
    });
  });
});


process.on('exit', function() {
  assert.strictEqual(src1.reads, 2);
  assert.strictEqual(src2.reads, 2);
});
