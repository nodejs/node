'use strict';
require('../common');
const http = require('node:http');
const assert = require('node:assert');

// Simple test to check if body size is less  than the content length set is

function test(server) {
  server.listen(0, () => {
    http.get({ port: server.address().port });
  });
}

// Test with message size longer than content-length header
{
  const server = http.createServer((req, res) => {
    assert.throws(() => {
      res.setHeader('content-length', 5);
      res.write('hello world!!');
    }, {
      name: 'Error',
      code: 'ERR_HTTP_CONTENT_LENGTH_EXCEEDED'
    });
    process.exit();
  });
  test(server);
}
