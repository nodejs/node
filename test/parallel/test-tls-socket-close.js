'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

let tlsSocket;
// tls server
const tlsServer = tls.createServer({ cert, key }, (socket) => {
  tlsSocket = socket;
  socket.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'EINVAL');
    tlsServer.close();
    netServer.close();
  }));
});

let netSocket;
// plain tcp server
const netServer = net.createServer((socket) => {
  // if client wants to use tls
  tlsServer.emit('connection', socket);

  netSocket = socket;
}).listen(0, common.mustCall(function() {
  connectClient(netServer);
}));

function connectClient(server) {
  const tlsConnection = tls.connect({
    host: 'localhost',
    port: server.address().port,
    rejectUnauthorized: false
  });

  tlsConnection.write('foo', 'utf8', common.mustCall(() => {
    assert(netSocket);
    netSocket.setTimeout(1, common.mustCall(() => {
      assert(tlsSocket);
      // this breaks if TLSSocket is already managing the socket:
      netSocket.destroy();
      const interval = setInterval(() => {
        // Checking this way allows us to do the write at a time that causes a
        // segmentation fault (not always, but often) in Node.js 7.7.3 and
        // earlier. If we instead, for example, wait on the `close` event, then
        // it will not segmentation fault, which is what this test is all about.
        if (tlsSocket._handle._parent.bytesRead === 0) {
          tlsSocket.write('bar');
          clearInterval(interval);
        }
      }, 1);
    }));
  }));
  tlsConnection.on('error', (e) => {
    // Tolerate the occasional ECONNRESET.
    // Ref: https://github.com/nodejs/node/issues/13184
    if (e.code !== 'ECONNRESET')
      throw e;
  });
}
