'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),

  // NOTE: Certificate Common Name is 'agent1'
  cert: fixtures.readKey('agent1-cert.pem'),

  // NOTE: TLS 1.3 creates new session ticket **after** handshake so
  // `getSession()` output will be different even if the session was reused
  // during the handshake.
  secureProtocol: 'TLSv1_2_method'
};

const ca = [ fixtures.readKey('ca1-cert.pem') ];

const server = https.createServer(options, function(req, res) {
  res.end('ok');
}).listen(0, common.mustCall(function() {
  const port = this.address().port;

  const req = https.get({
    port,
    path: '/',
    ca,
    servername: 'nodejs.org',
  }, common.mustNotCall());

  req.on('error', common.mustCall((err) => {
    assert.strictEqual(
      err.message,
      'Hostname/IP does not match certificate\'s altnames: ' +
        'Host: nodejs.org. is not cert\'s CN: agent1');

    const second = https.get({
      port,
      path: '/',
      ca,
      servername: 'nodejs.org',
    }, common.mustNotCall());

    second.on('error', common.mustCall((err) => {
      server.close();

      assert.strictEqual(
        err.message,
        'Hostname/IP does not match certificate\'s altnames: ' +
          'Host: nodejs.org. is not cert\'s CN: agent1');
    }));
  }));
}));
