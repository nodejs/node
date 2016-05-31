'use strict';
var common = require('../common');
var assert = require('assert');
var stream = require('stream');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

class TestWriter extends stream.Writable {
  _write(buffer, encoding, callback) {
    console.log('write called');
    // super slow write stream (callback never called)
  }
}

var dest = new TestWriter();

class TestReader extends stream.Readable {
  constructor() {
    super();
    this.reads = 0;
  }

  _read(size) {
    this.reads += 1;
    this.push(crypto.randomBytes(size));
  }
}

var src1 = new TestReader();
var src2 = new TestReader();

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
  assert.equal(src1.reads, 2);
  assert.equal(src2.reads, 2);
});
