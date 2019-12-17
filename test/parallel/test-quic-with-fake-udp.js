// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { makeUDPPair } = require('../common/udppair');
const assert = require('assert');
const quic = require('quic');
const { kUDPHandleForTesting } = require('internal/quic/core');

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { serverSide, clientSide } = makeUDPPair();

const server = quic.createSocket({
  endpoint: { port: 0 },
  validateAddress: true,
  [kUDPHandleForTesting]: serverSide._handle
});

serverSide.afterBind();
server.listen({
  key,
  cert,
  ca,
  rejectUnauthorized: false,
  maxCryptoBuffer: 4096,
  alpn: 'meow'
});

server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = session.openStream({ halfOpen: false });
    stream.end('Hi!');
    stream.on('data', common.mustNotCall());
    stream.on('finish', common.mustCall());
    stream.on('close', common.mustNotCall());
    stream.on('end', common.mustNotCall());
  }));

  session.on('close', common.mustNotCall());
}));

server.on('ready', common.mustCall(() => {
  const client = quic.createSocket({
    endpoint: { port: 0 },
    [kUDPHandleForTesting]: clientSide._handle,
    client: {
      key,
      cert,
      ca,
      alpn: 'meow'
    }
  });
  clientSide.afterBind();

  const req = client.connect({
    address: 'localhost',
    port: server.address.port
  });

  req.on('stream', common.mustCall((stream) => {
    stream.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'Hi!');
    }));

    stream.on('end', common.mustCall());
  }));

  req.on('close', common.mustNotCall());
}));

server.on('close', common.mustNotCall());
