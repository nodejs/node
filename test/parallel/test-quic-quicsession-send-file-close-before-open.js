// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { createQuicSocket } = require('net');
const fs = require('fs');

const { key, cert, ca } = require('../common/quic');

const server = createQuicSocket();

server.listen({ key, cert, ca, alpn: 'meow' });

server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = session.openStream({ halfOpen: false });

    fs.open = common.mustCall(fs.open);
    fs.close = common.mustCall(fs.close);

    stream.sendFile(__filename);
    stream.destroy();  // Destroy the stream before opening the fd finishes.

    session.close();
    server.close();
  }));

  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  const client = createQuicSocket({ client: { key, cert, ca, alpn: 'meow' } });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  req.on('stream', common.mustNotCall());

  req.on('close', common.mustCall(() => client.close()));
}));

server.on('close', common.mustCall());
