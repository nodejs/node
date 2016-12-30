'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');
const fs = require('fs');
const net = require('net');

var bonkers = Buffer.alloc(1024, 42);

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = net.createServer(common.mustCall(function(c) {
  setTimeout(common.mustCall(function() {
    var s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('_tlsError', common.mustCall(function() {}));

    s.on('close', function() {
      server.close();
      s.destroy();
    });
  }), 200);
})).listen(0, function() {
  var c = net.connect({port: this.address().port}, function() {
    c.write(bonkers);
  });
});
