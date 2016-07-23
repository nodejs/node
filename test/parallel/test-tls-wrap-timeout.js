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
  c.write('hello');
  c.destroy();
  server.close();
});

var socket;
server.listen(0, function() {
  socket = net.connect(this.address().port, function() {
    var s = socket.setTimeout(Number.MAX_VALUE, function() {
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

process.on('exit', () => {
  assert.strictEqual(socket._idleTimeout, -1);
});
