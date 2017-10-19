'use strict';

/**
 * This test covers http.Server({ ServerResponse }) option:
 * With ServerResponse option the server should use
 * the new class for creating res Object instead of the default
 * http.ServerResponse.
 */
const common = require('../common');
const assert = require('assert');
const http = require('http');

class MyServerResponse extends http.ServerResponse {
  status(code) {
    return this.writeHead(code, { 'Content-Type': 'text/plain' });
  }
}

const server = http.Server({
  ServerResponse: MyServerResponse
}, common.mustCall(function(req, res) {
  res.status(200);
  res.end();
}));
server.listen();

server.on('listening', function makeRequest() {
  http.get({ port: this.address().port }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', () => {
      server.close();
    });
    res.resume();
  });
});
