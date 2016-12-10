'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var sent = 'hello world';
var received = '';

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

    s.on('data', function(chunk) {
      received += chunk;
    });

    s.on('end', function() {
      server.close();
      s.destroy();
    });
  }, 200);
}).listen(0, function() {
  var c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, function() {
    c.end(sent);
  });
});

process.on('exit', function() {
  assert.equal(received, sent);
});
