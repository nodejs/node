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

const ws = new Writable({
  write: common.mustCall((chunk, encoding, callback) => {
    assert(chunk.toString().endsWith('hello world'));
    setImmediate(callback);
  })
});

res.socket = ws;
ws._httpMessage = res;
res.connection = ws;

res.end('hello world');
