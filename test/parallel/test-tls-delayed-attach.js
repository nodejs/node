'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const net = require('net');

const sent = 'hello world';
let received = '';

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = net.createServer(function(c) {
  setTimeout(function() {
    const s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('data', function(chunk) {
      received += chunk;
    });

    s.on('end', function() {
      server.close();
      s.destroy();
    });
  }, 200);
}).listen(0, function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, function() {
    c.end(sent);
  });
});

process.on('exit', function() {
  assert.strictEqual(received, sent);
});
