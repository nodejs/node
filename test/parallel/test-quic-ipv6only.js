// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasIPv6)
  common.skip('missing ipv6');

if (!common.hasQuic)
  common.skip('missing quic');

common.skip(
  'temporarily skip ipv6only check. dual stack support is current broken');

const assert = require('assert');
const { createQuicSocket } = require('net');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');

const kALPN = 'zzz';

// Connecting to ipv6 server using "127.0.0.1" should work when
// `ipv6Only` is set to `false`.
async function ipv6() {
  const server = createQuicSocket({ endpoint: { type: 'udp6' } });
  const client = createQuicSocket({ client: { key, cert, ca, alpn: kALPN } });

  server.listen({ key, cert, ca, alpn: kALPN });

  server.on('session', common.mustCall((serverSession) => {
    serverSession.on('stream', common.mustCall());
  }));

  await once(server, 'ready');

  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  await once(session, 'secure');

  const stream = session.openStream({ halfOpen: true });
  stream.end('hello');

  await once(stream, 'close');

  client.close();
  server.close();

  await Promise.allSettled([
    once(client, 'close'),
    once(server, 'close')
  ]);
}

// When the `ipv6Only` set to `true`, a client cann't connect to it
// through "127.0.0.1".
async function ipv6Only() {
  const server = createQuicSocket({ endpoint: { type: 'udp6-only' } });
  const client = createQuicSocket({ client: { key, cert, ca, alpn: kALPN } });

  server.listen({ key, cert, ca, alpn: kALPN });
  server.on('session', common.mustNotCall());

  await once(server, 'ready');

  // This will attempt to connect to the ipv4 localhost address
  // but should fail as the connection idle timeout expires.
  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
    idleTimeout: common.platformTimeout(1),
  });

  session.on('secure', common.mustNotCall());

  await once(session, 'close');

  client.close();
  server.close();

  await Promise.allSettled([
    once(client, 'close'),
    once(server, 'close')
  ]);
}

// Creating the QuicSession fails when connect type does not match the
// the connect IP address...
async function mismatch() {
  const server = createQuicSocket({ endpoint: { type: 'udp6' } });
  const client = createQuicSocket({ client: { key, cert, ca, alpn: kALPN } });

  server.listen({ key, cert, ca, alpn: kALPN });
  server.on('session', common.mustNotCall());

  await once(server, 'ready');

  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
    type: 'udp6',
    idleTimeout: common.platformTimeout(1),
  });

  session.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_OPERATION_FAILED');
    client.close();
    server.close();
  }));

  session.on('secure', common.mustNotCall());
  session.on('close', common.mustCall());

  await Promise.allSettled([
    once(client, 'close'),
    once(server, 'close')
  ]);
}

ipv6()
  .then(ipv6Only)
  .then(mismatch)
  .then(common.mustCall());
