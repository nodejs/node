// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const path = require('path');
const { createQuicSocket } = require('net');
const { once } = require('events');

const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'meow' };

const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

(async function() {
  server.on('session', common.mustCall(async (session) => {
    const stream = await session.openStream({ halfOpen: false });
    const nonexistentPath = path.resolve(__dirname, 'nonexistent.file');
    stream.on('error', common.expectsError({
      code: 'ENOENT',
      syscall: 'open',
      path: nonexistentPath
    }));
    stream.sendFile(nonexistentPath);
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
    server.close();
    client.close();
  }));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
