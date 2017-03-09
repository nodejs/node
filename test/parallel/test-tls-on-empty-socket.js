'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const net = require('net');

let out = '';

const server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  c.end('hello');
}).listen(0, function() {
  const socket = new net.Socket();

  const s = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, function() {
    s.on('data', function(chunk) {
      out += chunk;
    });
    s.on('end', function() {
      s.destroy();
      server.close();
    });
  });

  socket.connect(this.address().port);
});

process.on('exit', function() {
  assert.equal(out, 'hello');
});
