'use strict';

/**
 * This test covers http.Server({ IncomingMessage }) option:
 * With IncomingMessage option the server should use
 * the new class for creating req Object instead of the default
 * http.IncomingMessage.
 */
const common = require('../common');
const assert = require('assert');
const http = require('http');

class MyIncomingMessage extends http.IncomingMessage {
  getUserAgent() {
    return this.headers['user-agent'] || 'unknown';
  }
}

const server = http.createServer({
  IncomingMessage: MyIncomingMessage
}, common.mustCall(function(req, res) {
  assert.strictEqual(req.getUserAgent(), 'node-test');
  res.statusCode = 200;
  res.end();
}));
server.listen();

server.on('listening', function makeRequest() {
  http.get({
    port: this.address().port,
    headers: {
      'User-Agent': 'node-test'
    }
  }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', () => {
      server.close();
    });
    res.resume();
  });
});
