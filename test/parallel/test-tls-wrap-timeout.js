'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = tls.createServer(options, function(c) {
  setImmediate(function() {
    c.write('hello');
    setImmediate(function() {
      c.destroy();
      server.close();
    });
  });
});

server.listen(0, function() {
  var socket = net.connect(this.address().port, function() {
    var s = socket.setTimeout(common.platformTimeout(100), function() {
      throw new Error('timeout');
    });
    assert.ok(s instanceof net.Socket);

    var tsocket = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    tsocket.resume();
  });
});

// hold event loop open long enough to make sure socket timeout does not fire
setTimeout(function() {}, common.platformTimeout(300));
