'use strict';

// Synchronous zlib output collection must not depend on a user-modified
// Array.prototype.push.

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const zlib = require('zlib');

const expected = fixtures.readSync('person.jpg');
const compressed = fixtures.readSync('person.jpg.gz');

// Without options, the default chunk size still yields several chunks.
// A small chunkSize goes through the options path and yields many chunks.
for (const opts of [undefined, { chunkSize: 1024 }]) {
  const originalPush = Array.prototype.push;
  let result;
  Array.prototype.push = function() {
    throw new Error('Array.prototype.push should not be called');
  };
  try {
    result = zlib.gunzipSync(compressed, opts);
  } finally {
    Array.prototype.push = originalPush;
  }
  assert.deepStrictEqual(result, expected);
}
