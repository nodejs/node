// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that .connect() can be called multiple times with different servers.

const { createQuicSocket } = require('net');

const { key, cert, ca } = require('../common/quic');

const { once } = require('events');

(async function() {
  const servers = [];
  for (let i = 0; i < 3; i++) {
    const server = createQuicSocket();

    server.listen({ key, cert, ca, alpn: 'meow' });

    server.on('session', common.mustCall((session) => {
      session.on('secure', common.mustCall(() => {
        const stream = session.openStream({ halfOpen: true });
        stream.end('Hi!');
      }));
    }));

    server.on('close', common.mustCall());

    servers.push(server);
  }

  await Promise.all(servers.map((server) => once(server, 'ready')));

  const client = createQuicSocket({ client: { key, cert, ca, alpn: 'meow' } });

  for (const server of servers) {
    const req = client.connect({
      address: 'localhost',
      port: server.endpoints[0].address.port
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
