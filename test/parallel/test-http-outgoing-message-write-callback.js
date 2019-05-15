'use strict';

const common = require('../common');

// This test ensures that the callback of `OutgoingMessage.prototype.write()` is
// called also when writing empty chunks.

const assert = require('assert');
const http = require('http');
const stream = require('stream');

const expected = ['a', 'b', '', Buffer.alloc(0), 'c'];
const results = [];

const writable = new stream.Writable({
  write(chunk, encoding, callback) {
    setImmediate(callback);
  }
});

const res = new http.ServerResponse({
  method: 'GET',
  httpVersionMajor: 1,
  httpVersionMinor: 1
});

res.assignSocket(writable);

for (const chunk of expected) {
  res.write(chunk, () => {
    results.push(chunk);
  });
}

res.end(common.mustCall(() => {
  assert.deepStrictEqual(results, expected);
}));
