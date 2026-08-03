'use strict';

/**
 * This test covers http.Server({ ServerResponse }) option:
 * With ServerResponse option the server should use
 * the new class for creating res Object instead of the default
 * http.ServerResponse.
 */
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http = require('http');
const https = require('https');

class MyServerResponse extends http.ServerResponse {
  status(code) {
    return this.writeHead(code, { 'Content-Type': 'text/plain' });
  }
}

const server = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ca: fixtures.readKey('ca1-cert.pem'),
  ServerResponse: MyServerResponse
}, common.mustCall(function(req, res) {
  res.status(200);
  res.end();
}));
server.listen();

server.on('listening', function makeRequest() {
  https.get({
    port: this.address().port,
    rejectUnauthorized: false
  }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', () => {
      server.close();
    });
    res.resume();
  });
});
