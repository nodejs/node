'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that .connect() can be called multiple times with different servers.

const quic = require('quic');

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { once } = require('events');

(async function() {
  const servers = [];
  for (let i = 0; i < 3; i++) {
    const server = quic.createSocket({ port: 0, validateAddress: true });

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
        const stream = session.openStream({ halfOpen: true });
        stream.end('Hi!');
      }));
    }));

    server.on('close', common.mustCall());

    servers.push(server);
  }

  await Promise.all(servers.map((server) => once(server, 'ready')));

  const client = quic.createSocket({
    port: 0,
    client: {
      key,
      cert,
      ca,
      alpn: 'meow'
    }
  });

  for (const server of servers) {
    const req = client.connect({
      address: 'localhost',
      port: server.address.port
    });

    const [ stream ] = await once(req, 'stream');
    stream.resume();
    await once(stream, 'end');

    server.close();
    req.close();
    await once(req, 'close');
  }

  client.close();

  await once(client, 'close');
})().then(common.mustCall());
