// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasIPv6)
  common.skip('missing ipv6');

if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');

const kALPN = 'zzz';

// Setting `type` to `udp4` while setting `ipv6Only` to `true` is possible
// and it will throw an error.
{
  const server = createQuicSocket({
    endpoint: {
      type: 'udp4',
      ipv6Only: true
    } });

  server.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EINVAL');
    assert.strictEqual(err.message, 'bind EINVAL 0.0.0.0');
  }));

  server.listen({ key, cert, ca, alpn: kALPN });
}

// Connecting ipv6 server by "127.0.0.1" should work when `ipv6Only`
// is set to `false`.
(async () => {
  const server = createQuicSocket({
    endpoint: {
      type: 'udp6',
      ipv6Only: false
    } });
  const client = createQuicSocket({ client: { key, cert, ca, alpn: kALPN } });

  server.listen({ key, cert, ca, alpn: kALPN });

  server.on('session', common.mustCall((serverSession) => {
    serverSession.on('stream', common.mustCall());
  }));

  await once(server, 'ready');

  const session = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
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
})().then(common.mustCall());

// When the `ipv6Only` set to `true`, a client cann't connect to it
// through "127.0.0.1".
(async () => {
  const server = createQuicSocket({
    endpoint: {
      type: 'udp6',
      ipv6Only: true
    } });
  const client = createQuicSocket({ client: { key, cert, ca, alpn: kALPN } });

  server.listen({ key, cert, ca, alpn: kALPN });
  server.on('session', common.mustNotCall());

  await once(server, 'ready');

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
})();

// Creating the QuicSession fails when connect type does not match the
// the connect IP address...
(async () => {
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
    assert.strictEqual(err.code, 'ERR_QUICCLIENTSESSION_FAILED');
    client.close();
    server.close();
  }));

  session.on('secure', common.mustNotCall());
  session.on('close', common.mustCall());

  await Promise.allSettled([
    once(client, 'close'),
    once(server, 'close')
  ]);
})().then(common.mustCall());
