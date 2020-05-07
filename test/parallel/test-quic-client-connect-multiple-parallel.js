// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that .connect() can be called multiple times with different servers.

const assert = require('assert');
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

  let done = 0;
  for (const server of servers) {
    const req = client.connect({
      address: 'localhost',
      port: server.endpoints[0].address.port
    });

    req.on('stream', common.mustCall((stream) => {
      stream.on('data', common.mustCall(
        (chk) => assert.strictEqual(chk.toString(), 'Hi!')));
      stream.on('end', common.mustCall(() => {
        server.close();
        req.close();
        if (++done === servers.length) client.close();
      }));
    }));

    req.on('close', common.mustCall());
  }
})().then(common.mustCall());
