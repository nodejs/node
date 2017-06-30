'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fs = require('fs');
const net = require('net');

const bonkers = Buffer.alloc(1024, 42);

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
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
