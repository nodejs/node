'use strict';

// A socket that failed during the handshake does not complete it when the
// callback of the `newSession` event is called afterwards.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const { constants } = require('crypto');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = tls.createServer({
  key,
  cert,
  maxVersion: 'TLSv1.2',
  secureOptions: constants.SSL_OP_NO_TICKET,
});

// An invalid TLS record is appended to the client chunk that follows the
// ClientHello, which is expected to carry ClientKeyExchange, ChangeCipherSpec
// and Finished in a single `data` event. The server then fails with a TLS
// error after it has requested the callback of the `newSession` event.
const invalidRecord = Buffer.from([0x17, 0x03, 0x03, 0x00, 0x05, 1, 2, 3, 4, 5]);
const proxy = net.createServer(common.mustCall((downstream) => {
  const upstream = net.connect(server.address().port);
  let first = true;
  let injected = false;
  downstream.on('data', (chunk) => {
    // Handshake records have the content type 0x16.
    if (!first && !injected && chunk[0] === 0x16) {
      injected = true;
      chunk = Buffer.concat([chunk, invalidRecord]);
    }
    first = false;
    upstream.write(chunk);
  });
  upstream.pipe(downstream);
  downstream.on('error', () => {});
  upstream.on('error', () => {});
  downstream.on('close', () => upstream.destroy());
  upstream.on('close', () => downstream.destroy());
}));

let done;
server.on('secureConnection', common.mustNotCall());
server.on('newSession', common.mustCall((id, data, callback) => {
  done = callback;
}));
server.on('tlsClientError', common.mustCall((err, socket) => {
  assert.ok(socket.destroyed);
  socket.on('secure', common.mustNotCall());
  done();
  setImmediate(common.mustCall(() => {
    proxy.close();
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  proxy.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: proxy.address().port,
      maxVersion: 'TLSv1.2',
      secureOptions: constants.SSL_OP_NO_TICKET,
      rejectUnauthorized: false,
    });
    client.on('error', () => {});
  }));
}));
