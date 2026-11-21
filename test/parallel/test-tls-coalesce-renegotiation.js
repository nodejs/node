'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('BoringSSL does not support renegotiation');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// Deliver application data and a TLS 1.2 ClientHello in one transport write.
// The data handler must run before renegotiation starts, so it can disable it.
const payload = Buffer.from('disable renegotiation');
let holding = false;
let client;

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  maxVersion: 'TLSv1.2',
}, common.mustCall((socket) => {
  socket.on('data', common.mustCall((data) => {
    assert.deepStrictEqual(data, payload);
    socket.disableRenegotiation();
  }));
  socket.on('secure', common.mustNotCall('renegotiation completed'));
  socket.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_TLS_RENEGOTIATION_DISABLED');
    client.destroy();
    socket.destroy();
    server.close();
    proxy.close();
  }));
}));

const proxy = net.createServer(common.mustCall((socket) => {
  const upstream = net.connect(server.address().port);
  upstream.pipe(socket);
  socket.on('end', () => upstream.end());
  socket.on('close', () => upstream.destroy());
  const held = [];
  let pending = Buffer.alloc(0);
  socket.on('data', (data) => {
    if (!holding) {
      upstream.write(data);
      return;
    }
    held.push(data);
    pending = Buffer.concat([pending, data]);
    while (pending.length >= 5) {
      const length = pending.readUInt16BE(3);
      if (pending.length < 5 + length)
        break;
      if (pending[0] === 22) {  // handshake
        holding = false;
        upstream.write(Buffer.concat(held));
        return;
      }
      pending = pending.subarray(5 + length);
    }
  });
}));

server.listen(0, common.mustCall(() => {
  proxy.listen(0, common.mustCall(() => {
    client = tls.connect({
      port: proxy.address().port,
      ca: fixtures.readKey('ca1-cert.pem'),
      servername: 'agent1',
      maxVersion: 'TLSv1.2',
    }, common.mustCall(() => {
      holding = true;
      client.write(payload, common.mustCall(() => {
        assert(client.renegotiate({},
                                  common.mustNotCall('renegotiation completed')));
      }));
    }));
  }));
}));
