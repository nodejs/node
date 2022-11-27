'use strict';
const common = require('../common');
const { Writable } = require('stream');

const assert = require('assert');
const http = require('http');

// Check if Writable.toWeb works on the response object after creating a server.
const server = http.createServer((req, res) => {
  const webStreamResponse = Writable.toWeb(res);
  assert.strictEqual(webStreamResponse instanceof Writable, true);
});

server.listen(
  0,
  common.mustCall(() => {
    server.close();
  })
);
