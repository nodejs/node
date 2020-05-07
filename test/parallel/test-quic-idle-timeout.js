// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');

const kALPN = 'zzz';
const idleTimeout = common.platformTimeout(1);
const options = { key, cert, ca, alpn: kALPN };

// Test idleTimeout. The test will hang and fail with a timeout
// if the idleTimeout is not working correctly.

(async () => {
  const server = createQuicSocket({ server: options });
  const client = createQuicSocket({ client: options });

  server.listen();
  server.on('session', common.mustCall());

  await once(server, 'ready');

  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
    idleTimeout,
  });

  await once(session, 'close');

  assert(session.idleTimeout);
  client.close();
  server.close();

  await Promise.all([
    once(client, 'close'),
    once(server, 'close')
  ]);
})().then(common.mustCall());


(async () => {
  const server = createQuicSocket({ server: options });
  const client = createQuicSocket({ client: options });

  server.listen({ idleTimeout });

  server.on('session', common.mustCall(async (session) => {
    await once(session, 'close');
    assert(session.idleTimeout);
    client.close();
    server.close();
    await Promise.all([
      once(client, 'close'),
      once(server, 'close')
    ]);
  }));

  await once(server, 'ready');

  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  await once(session, 'close');
})().then(common.mustCall());
