'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const net = require('net');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const server = tls.createServer(options, function(c) {
  setTimeout(function() {
    c.write('hello');
    setTimeout(function() {
      c.destroy();
      server.close();
    }, 150);
  }, 150);
});

server.listen(0, function() {
  const socket = net.connect(this.address().port, function() {
    const s = socket.setTimeout(common.platformTimeout(240), function() {
      throw new Error('timeout');
    });
    assert.ok(s instanceof net.Socket);

    const tsocket = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    tsocket.resume();
  });
});
