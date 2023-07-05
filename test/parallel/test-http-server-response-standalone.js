'use strict';

const common = require('../common');
const { ServerResponse } = require('http');
const { Writable } = require('stream');
const assert = require('assert');

// Check that ServerResponse can be used without a proper Socket
// Refs: https://github.com/nodejs/node/issues/14386
// Refs: https://github.com/nodejs/node/issues/14381

const res = new ServerResponse({
  method: 'GET',
  httpVersionMajor: 1,
  httpVersionMinor: 1
});

let firstChunk = true;

const ws = new Writable({
  write: common.mustCall((chunk, encoding, callback) => {
    if (firstChunk) {
      assert(chunk.toString().endsWith('hello world'));
      firstChunk = false;
    } else {
      assert.strictEqual(chunk.length, 0);
    }
    setImmediate(callback);
  }, 2)
});

res.assignSocket(ws);

assert.throws(function() {
  res.assignSocket(ws);
}, {
  code: 'ERR_HTTP_SOCKET_ASSIGNED'
});

res.end('hello world');
