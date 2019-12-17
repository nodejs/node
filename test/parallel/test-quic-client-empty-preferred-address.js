'use strict';

// This test ensures that when we don't define `preferredAddress`
// on the server while the `preferredAddressPolicy` on the client
// is `accpet`, it works as expected.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { createSocket } = require('quic');

const kALPN = 'zzz';
const kServerName = 'agent2';
const server = createSocket({
  endpoint: { port: 0 },
  validateAddress: true
});

let client;
server.listen({
  key,
  cert,
  ca,
  alpn: kALPN,
});

server.on('session', common.mustCall((serverSession) => {
  serverSession.on('stream', common.mustCall((stream) => {
    stream.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString('utf8'), 'hello');
    }));
    stream.on('end', common.mustCall(() => {
      stream.close();
      client.close();
      server.close();
    }));
  }));
}));

server.on('ready', common.mustCall(() => {
  client = createSocket({ endpoint: { port: 0 } });

  const clientSession = client.connect({
    address: 'localhost',
    key,
    cert,
    ca,
    alpn: kALPN,
    port: server.address.port,
    servername: kServerName,
    preferredAddressPolicy: 'accept',
  });

  clientSession.on('close', common.mustCall());

  clientSession.on('secure', common.mustCall(() => {
    const stream = clientSession.openStream();
    stream.end('hello');
    stream.on('close', common.mustCall());
  }));
}));
