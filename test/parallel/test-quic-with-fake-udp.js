// Flags: --expose-internals --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Tests that QUIC works properly when using a pair of mocked UDP ports.

const { makeUDPPair } = require('../common/udppair');
const assert = require('assert');
const { createQuicSocket } = require('net');
const { kUDPHandleForTesting } = require('internal/quic/core');

const { key, cert, ca } = require('../common/quic');

const { serverSide, clientSide } = makeUDPPair();

const server = createQuicSocket({
  endpoint: { [kUDPHandleForTesting]: serverSide._handle }
});

serverSide.afterBind();
server.listen({ key, cert, ca, alpn: 'meow' });

server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall(() => {
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
  const client = createQuicSocket({
    endpoint: { [kUDPHandleForTesting]: clientSide._handle },
    client: { key, cert, ca, alpn: 'meow' }
  });
  clientSide.afterBind();

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
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
