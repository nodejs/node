'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const net = require('net');
const fixtures = require('../common/fixtures');

const bonkers = Buffer.alloc(1024, 42);

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = net.createServer(common.mustCall(function(c) {
  setTimeout(common.mustCall(function() {
    const s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('_tlsError', common.mustCall());

    s.on('close', function() {
      server.close();
      s.destroy();
    });
  }), 200);
})).listen(0, function() {
  const c = net.connect({port: this.address().port}, function() {
    c.write(bonkers);
  });
});
