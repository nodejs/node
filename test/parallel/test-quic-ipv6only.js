// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasIPv6)
  common.skip('missing ipv6');

if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createSocket } = require('quic');
const { key, cert, ca } = require('../common/quic');

const kServerName = 'agent2';
const kALPN = 'zzz';

// Setting `type` to `udp4` while setting `ipv6Only` to `true` is possible
// and it will throw an error.
{
  const server = createSocket({ endpoint: { type: 'udp4', ipv6Only: true } });

  server.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EINVAL');
    assert.strictEqual(err.message, 'bind EINVAL 0.0.0.0');
  }));

  server.listen({ key, cert, ca, alpn: kALPN });
}

// Connecting ipv6 server by "127.0.0.1" should work when `ipv6Only`
// is set to `false`.
{
  const server = createSocket({ endpoint: { type: 'udp6', ipv6Only: false } });

  server.listen({ key, cert, ca, alpn: kALPN });

  server.on('session', common.mustCall((serverSession) => {
    serverSession.on('stream', common.mustCall());
  }));

  server.on('ready', common.mustCall(() => {
    const client = createSocket({ client: { key, cert, ca, alpn: kALPN } });

    const clientSession = client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      servername: kServerName,
    });

    clientSession.on('secure', common.mustCall(() => {
      const clientStream = clientSession.openStream({ halfOpen: true });
      clientStream.end('hello');
      clientStream.on('close', common.mustCall(() => {
        client.close();
        server.close();
      }));
    }));
  }));
}

// When the `ipv6Only` set to `true`, a client cann't connect to it
// through "127.0.0.1".
{
  const server = createSocket({ endpoint: { type: 'udp6', ipv6Only: true } });

  server.listen({ key, cert, ca, alpn: kALPN });
  server.on('session', common.mustNotCall());

  server.on('ready', common.mustCall(() => {
    const client = createSocket({ client: { key, cert, ca, alpn: kALPN } });

    client.on('ready', common.mustCall());

    const clientSession = client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      servername: kServerName,
      idleTimeout: common.platformTimeout(1),
    });

    clientSession.on('secure', common.mustNotCall());
    clientSession.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// Creating the QuicSession fails when connect type does not match the
// the connect IP address...
{
  const server = createSocket({ endpoint: { type: 'udp6' } });

  server.listen({ key, cert, ca, alpn: kALPN });
  server.on('session', common.mustNotCall());

  server.on('ready', common.mustCall(() => {
    const client = createSocket({ client: { key, cert, ca, alpn: kALPN } });

    client.on('ready', common.mustCall());

    const clientSession = client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      type: 'udp6',
      servername: kServerName,
      idleTimeout: common.platformTimeout(1),
    });

    clientSession.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_QUICCLIENTSESSION_FAILED');
    }));

    clientSession.on('secure', common.mustNotCall());
    clientSession.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
