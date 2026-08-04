'use strict';

require('../common');

const assert = require('assert');
const stream = require('stream');

const writable = new stream.Writable();

function testStates(ending, finished, ended) {
  assert.strictEqual(writable._writableState.ending, ending);
  assert.strictEqual(writable._writableState.finished, finished);
  assert.strictEqual(writable._writableState.ended, ended);
}

writable._write = (chunk, encoding, cb) => {
  // Ending, finished, ended start in false.
  testStates(false, false, false);
  cb();
};

writable.on('finish', () => {
  // Ending, finished, ended = true.
  testStates(true, true, true);
});

const result = writable.end('testing function end()', () => {
  // Ending, finished, ended = true.
  testStates(true, true, true);
});

// End returns the writable instance
assert.strictEqual(result, writable);

// Ending, ended = true.
// finished = false.
testStates(true, false, true);
