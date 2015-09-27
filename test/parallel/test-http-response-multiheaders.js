'use strict';

const http = require('http');
const common = require('../common');
const assert = require('assert');

// Test that certain response header fields do not repeat
const norepeat = [
  'retry-after',
  'etag',
  'last-modified',
  'server',
  'age',
  'expires'
];

const server = http.createServer(function(req, res) {
  for (let name of norepeat) {
    res.setHeader(name, ['A', 'B']);
  }
  res.setHeader('X-A', ['A', 'B']);
  res.end('ok');
});

server.listen(common.PORT, function() {
  http.get({port:common.PORT}, function(res) {
    for (let name of norepeat) {
      assert.equal(res.headers[name], 'A');
    }
    assert.equal(res.headers['x-a'], 'A, B');
    server.close();
  });
});
