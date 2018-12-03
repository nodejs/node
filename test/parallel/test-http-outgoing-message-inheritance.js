'use strict';

const common = require('../common');
const { OutgoingMessage } = require('http');
const { Writable } = require('stream');
const assert = require('assert');

// Check that OutgoingMessage can be used without a proper Socket
// Fixes: https://github.com/nodejs/node/issues/14386
// Fixes: https://github.com/nodejs/node/issues/14381

class Response extends OutgoingMessage {
  constructor() {
    super({ method: 'GET', httpVersionMajor: 1, httpVersionMinor: 1 });
  }

  _implicitHeader() {}
}

const res = new Response();
const ws = new Writable({
  write: common.mustCall((chunk, encoding, callback) => {
    assert(chunk.toString().match(/hello world/));
    setImmediate(callback);
  })
});

res.socket = ws;
ws._httpMessage = res;
res.connection = ws;

res.end('hello world');
