'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const net = require('net');
const tls = require('tls');
const { duplexPair } = require('stream');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

// Server sockets over a plain connection expose that net.Socket.
{
  const server = tls.createServer({ key, cert });
  let rawSocket;
  server.prependListener('connection', common.mustCall((socket) => {
    rawSocket = socket;
  }));
  server.on('secureConnection', common.mustCall((tlsSocket) => {
    assert.ok(rawSocket instanceof net.Socket);
    assert.strictEqual(tlsSocket.socket, rawSocket);
    tlsSocket.end();
    server.close();
  }));
  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      assert.strictEqual(client.socket, null);
      client.end();
    }));
  }));
}

// Server sockets over any other stream expose that stream, and still do after
// a failed handshake has destroyed them.
{
  const server = tls.createServer({ key, cert, minVersion: 'TLSv1.3' });
  const [serverSide, clientSide] = duplexPair();
  server.on('secureConnection', common.mustNotCall());
  server.on('tlsClientError', common.mustCall((err, tlsSocket) => {
    assert.strictEqual(tlsSocket.socket, serverSide);
    setImmediate(common.mustCall(() => {
      assert.strictEqual(tlsSocket.destroyed, true);
      assert.strictEqual(tlsSocket.socket, serverSide);
    }));
  }));
  server.emit('connection', serverSide);

  const client = tls.connect({ socket: clientSide, maxVersion: 'TLSv1.2' });
  assert.strictEqual(client.socket, clientSide);
  client.on('error', common.mustCall());
}
