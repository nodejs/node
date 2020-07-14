// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createQuicSocket } = require('net');
const { once } = require('events');
const fs = require('fs');

const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'meow' };

const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

(async function() {
  server.on('session', common.mustCall(async (session) => {
    const stream = await session.openStream({ halfOpen: false });

    fs.open = common.mustCall(fs.open);
    fs.close = common.mustCall(fs.close);

    stream.sendFile(__filename);
    stream.destroy();  // Destroy the stream before opening the fd finishes.

    session.close();

    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  req.on('stream', common.mustNotCall());

  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());

server.on('close', common.mustCall());
