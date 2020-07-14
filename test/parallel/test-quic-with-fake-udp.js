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

const options = { key, cert, ca, alpn: 'meow' };

const { serverSide, clientSide } = makeUDPPair();

const server = createQuicSocket({
  endpoint: { [kUDPHandleForTesting]: serverSide._handle },
  server: options
});
serverSide.afterBind();

const client = createQuicSocket({
  endpoint: { [kUDPHandleForTesting]: clientSide._handle },
  client: options
});
clientSide.afterBind();

(async function() {
  server.on('session', common.mustCall(async (session) => {
    session.on('close', common.mustNotCall());
    const stream = await session.openStream({ halfOpen: false });
    stream.end('Hi!');
    stream.on('data', common.mustNotCall());
    stream.on('finish', common.mustCall());
    stream.on('close', common.mustNotCall());
    stream.on('end', common.mustNotCall());
  }));

  await server.listen();

  const req = await client.connect({
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

})().then(common.mustCall());
