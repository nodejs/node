'use strict';
const common = require('../common');
const assert = require('assert');

const tls = require('tls');
const fs = require('fs');
const net = require('net');

const key = fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
const cert = fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem');

const T = 100;

// tls server
const tlsServer = tls.createServer({ cert, key }, (socket) => {
  setTimeout(() => {
    socket.on('error', (error) => {
      assert.strictEqual(error.code, 'EINVAL');
      tlsServer.close();
      netServer.close();
    });
    socket.write('bar');
  }, T * 2);
});

// plain tcp server
const netServer = net.createServer((socket) => {
    // if client wants to use tls
  tlsServer.emit('connection', socket);

  socket.setTimeout(T, () => {
    // this breaks if TLSSocket is already managing the socket:
    socket.destroy();
  });
}).listen(0, common.mustCall(function() {

  // connect client
  tls.connect({
    host: 'localhost',
    port: this.address().port,
    rejectUnauthorized: false
  }).write('foo');
}));
