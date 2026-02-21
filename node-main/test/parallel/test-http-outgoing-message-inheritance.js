'use strict';

const common = require('../common');
const { OutgoingMessage } = require('http');
const { Writable } = require('stream');
const assert = require('assert');

// Check that OutgoingMessage can be used without a proper Socket
// Refs: https://github.com/nodejs/node/issues/14386
// Refs: https://github.com/nodejs/node/issues/14381

class Response extends OutgoingMessage {
  _implicitHeader() {}
}

const res = new Response();

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

res.socket = ws;
ws._httpMessage = res;
res.connection = ws;

res.end('hello world');
