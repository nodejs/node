// Flags: --experimental-dtls --no-warnings

// Test: ALPN negotiation in DTLS.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = fixtures.readKey('agent1-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const serverAlpnChecked = Promise.withResolvers();

const endpoint = listen(mustCall(async (session) => {
  await session.opened;
  // Server should see the negotiated ALPN protocol.
  assert.strictEqual(session.alpnProtocol, 'coap');
  serverAlpnChecked.resolve();
}), {
  cert: serverCert.toString(),
  key: serverKey.toString(),
  port: 0,
  host: '127.0.0.1',
  alpn: ['coap', 'h2'],
});

const session = connect('127.0.0.1', endpoint.address.port, {
  ca: [ca.toString()],
  rejectUnauthorized: false,
  alpn: ['coap'],
});

await session.opened;

// Client should see the negotiated protocol.
assert.strictEqual(session.alpnProtocol, 'coap');

await serverAlpnChecked.promise;

await session.close();
await endpoint.close();

// ALPN with no protocol in common: RFC 7301 section 3.2 requires the server to
// send a fatal no_application_protocol alert, so the handshake must fail.
//
// This previously asserted the opposite -- that the handshake completed with
// neither peer reporting a protocol -- which left both ends connected with no
// agreement on what to speak. The callback returned SSL_TLSEXT_ERR_NOACK; it
// now returns SSL_TLSEXT_ERR_ALERT_FATAL, matching node:tls.
{
  const server = listen(() => {}, {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
    alpn: ['bar'],
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
    alpn: ['foo'],
  });

  await assert.rejects(
    client.opened,
    (err) => {
      assert.match(err.message, /no application protocol/);
      return true;
    },
    'a client offering no protocol the server supports must be rejected');

  await server.close();
}

// A server with no ALPN configured must not reject a client that offers
// protocols. The alert above applies only where the server does ALPN and
// finds no overlap; declining the extension entirely is not a failure.
{
  const gotServerSession = Promise.withResolvers();

  const server = listen(mustCall((s) => gotServerSession.resolve(s)), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
    alpn: ['foo'],
  });

  await client.opened;
  const serverSession = await gotServerSession.promise;
  await serverSession.opened;

  assert.strictEqual(client.alpnProtocol, undefined);
  assert.strictEqual(serverSession.alpnProtocol, undefined);

  await client.close();
  await server.close();
}

// A server that does ALPN must still accept a client that offers none: the
// selection callback only runs when the client sent the extension.
{
  const gotServerSession = Promise.withResolvers();

  const server = listen(mustCall((s) => gotServerSession.resolve(s)), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
    alpn: ['bar'],
  });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
  });

  await client.opened;
  const serverSession = await gotServerSession.promise;
  await serverSession.opened;

  assert.strictEqual(client.alpnProtocol, undefined);

  await client.close();
  await server.close();
}
