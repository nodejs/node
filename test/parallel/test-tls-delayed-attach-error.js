'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');
var fs = require('fs');
var net = require('net');

var bonkers = new Buffer(1024);
bonkers.fill(42);

var receivedError = false;
var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = net.createServer(function(c) {
  setTimeout(function() {
    var s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('_tlsError', function() {
      receivedError = true;
    });

    s.on('close', function() {
      server.close();
      s.destroy();
    });
  }, 200);
}).listen(common.PORT, function() {
  var c = net.connect({port: common.PORT}, function() {
    c.write(bonkers);
  });
});

process.on('exit', function() {
  assert.ok(receivedError);
});
