// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const quic = require('quic');
const fs = require('fs');

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const server = quic.createSocket({ validateAddress: true });

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
  const client = quic.createSocket({ client: { key, cert, ca, alpn: 'meow' } });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  req.on('stream', common.mustNotCall());

  req.on('close', common.mustCall(() => client.close()));
}));

server.on('close', common.mustCall());
