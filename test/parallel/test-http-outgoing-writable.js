'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const Stream = require('stream');
const { Writable } = Stream;

const server = http.createServer(common.mustCall((req, res) => {
  assert(res instanceof Stream, 'the response must be an instance of Stream');
  assert(res instanceof Writable,
         'the response must be an instance of Writable');
  assert.strictEqual(typeof res._write, 'function', '_write is a function');
  res.end();
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const req = http.get(server.address());
  assert(req instanceof Writable,
         'the request must be an instance of Writable');
  assert.strictEqual(typeof req._write, 'function', '_write is a function');
}));
