'use strict';

/**
 * This test covers http.Server({ IncomingMessage }) option:
 * With IncomingMessage option the server should use
 * the new class for creating req Object instead of the default
 * http.IncomingMessage.
 */
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http = require('http');
const https = require('https');

class MyIncomingMessage extends http.IncomingMessage {
  getUserAgent() {
    return this.headers['user-agent'] || 'unknown';
  }
}

const server = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ca: fixtures.readKey('ca1-cert.pem'),
  IncomingMessage: MyIncomingMessage
}, common.mustCall(function(req, res) {
  assert.strictEqual(req.getUserAgent(), 'node-test');
  res.statusCode = 200;
  res.end();
}));
server.listen();

server.on('listening', function makeRequest() {
  https.get({
    port: this.address().port,
    rejectUnauthorized: false,
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
