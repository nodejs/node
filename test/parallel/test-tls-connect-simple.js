'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var clientConnected = 0;
var serverConnected = 0;
var serverCloseCallbacks = 0;
var serverCloseEvents = 0;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = tls.Server(options, function(socket) {
  if (++serverConnected === 2) {
    server.close(function() {
      ++serverCloseCallbacks;
    });
    server.on('close', function() {
      ++serverCloseEvents;
    });
  }
});

server.listen(0, function() {
  var client1 = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    ++clientConnected;
    client1.end();
  });

  var client2 = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });
  client2.on('secureConnect', function() {
    ++clientConnected;
    client2.end();
  });
});

process.on('exit', function() {
  assert.equal(clientConnected, 2);
  assert.equal(serverConnected, 2);
  assert.equal(serverCloseCallbacks, 1);
  assert.equal(serverCloseEvents, 1);
});
