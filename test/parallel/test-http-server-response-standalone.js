'use strict';

const common = require('../common');
const { ServerResponse } = require('http');
const { Writable } = require('stream');
const assert = require('assert');

// check that ServerResponse can be used without a proper Socket
// Fixes: https://github.com/nodejs/node/issues/14386
// Fixes: https://github.com/nodejs/node/issues/14381

const res = new ServerResponse({
  method: 'GET',
  httpVersionMajor: 1,
  httpVersionMinor: 1
});

const ws = new Writable({
  write: common.mustCall((chunk, encoding, callback) => {
    assert(chunk.toString().match(/hello world/));
    setImmediate(callback);
  })
});

res.assignSocket(ws);

res.end('hello world');
