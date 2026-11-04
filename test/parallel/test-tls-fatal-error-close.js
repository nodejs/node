'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  minVersion: 'TLSv1.3',
  maxVersion: 'TLSv1.3',
}, common.mustCall((socket) => {
  const remotePort = socket.remotePort;
  let errorEmitted = false;
  socket.on('error', common.mustCall((err) => {
    assert.match(err.code, /^ERR_SSL_/);
    assert(socket.destroyed);
    assert.strictEqual(socket.errored, err);
    // Destroy the socket while preserving connection details for this handler.
    assert.strictEqual(socket.remoteAddress, '127.0.0.1');
    assert.strictEqual(socket.remoteFamily, 'IPv4');
    assert.strictEqual(socket.remotePort, remotePort);
    assert.strictEqual(socket.getProtocol(), 'TLSv1.3');
    errorEmitted = true;
  }));
  socket.on('close', common.mustCall(() => {
    assert(errorEmitted);
    assert(socket.destroyed);
    assert.strictEqual(socket.getProtocol(), null);
    client.destroy();
    server.close(common.mustCall());
  }));
}));
server.on('tlsClientError', common.mustNotCall());

let client;
server.listen(0, '127.0.0.1', common.mustCall(() => {
  const transport = net.connect(server.address().port, '127.0.0.1');
  client = tls.connect({
    socket: transport,
    rejectUnauthorized: false,
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
  });
  client.on('error', () => {});
  client.on('close', common.mustCall());
  server.once('secureConnection', common.mustCall(() => {
    // Corrupt an application-data record without closing the TCP connection.
    // The server must close on the TLS error without application cleanup.
    const record = Buffer.alloc(37);
    record.set([23, 3, 3, 0, 32]);
    transport.write(record);
  }));
}));
