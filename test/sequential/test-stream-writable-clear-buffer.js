'use strict';
const common = require('../common');
const Stream = require('stream');
// This test ensures that the _writeableState.bufferedRequestCount and
// the actual buffered request count are the same
const assert = require('assert');

class StreamWritable extends Stream.Writable {
  constructor() {
    super({ objectMode: true });
  }

  // We need a timeout like on the original issue thread
  // otherwise the code will never reach our test case
  // this means this should go on the sequential folder.
  _write(chunk, encoding, cb) {
    setTimeout(cb, common.platformTimeout(10));
  }
}

const testStream = new StreamWritable();
testStream.cork();

for (let i = 1; i <= 5; i++) {
  testStream.write(i, function() {
    assert.strictEqual(
      testStream._writableState.bufferedRequestCount,
      testStream._writableState.getBuffer().length,
      'bufferedRequestCount variable is different from the actual length of' +
      ' the buffer');
  });
}

testStream.end();
