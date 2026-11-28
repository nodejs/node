// Flags: --experimental-dtls --no-warnings

// Test: features that behave differently, or are reached by a different code
// path, when DTLS 1.3 is negotiated.
//
// A DTLS 1.3 server does its address validation with a pending handshake
// that only becomes a session once the cookie verifies, so everything the
// handshake reports after that point -- keylog lines, SNI, PSK -- has to
// reach a session JavaScript has not been given yet. Resumption moves after
// the handshake, the exporter is HKDF based, and the record limit is one
// byte lower.

import { hasCrypto, mustCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, createSecureContext, listen } = await import('node:dtls');

const HOST = '127.0.0.1';
const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();
const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// Connect and return the client and the server's session for it.
async function pair(serverOptions, clientOptions, onsession = () => {}) {
  const serverSession = Promise.withResolvers();
  const endpoint = listen((session) => {
    onsession(session);
    serverSession.resolve(session);
  }, { host: HOST, port: 0, ...V13, ...serverOptions });
  const client = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...V13, ...clientOptions,
  });
  await client.opened;
  const server = await serverSession.promise;
  await server.opened;
  assert.strictEqual(client.protocol, 'DTLSv1.3');
  return { endpoint, client, server };
}

// The server's handshake secrets are derived while the cookie-bearing
// ClientHello is processed, before the session is announced. They are held
// and delivered once a listener is attached in onsession.
{
  const serverLines = [];
  const { endpoint, client } = await pair({ cert, key }, {}, (session) => {
    session.onkeylog = (line) => serverLines.push(line.toString());
  });
  await client.close();
  await endpoint.close();

  const labels = (lines) => new Set(lines.map((l) => l.split(' ')[0]));
  for (const label of [
    'CLIENT_HANDSHAKE_TRAFFIC_SECRET',
    'SERVER_HANDSHAKE_TRAFFIC_SECRET',
    'CLIENT_TRAFFIC_SECRET_0',
    'SERVER_TRAFFIC_SECRET_0',
    'EXPORTER_SECRET',
  ]) {
    assert.ok(labels(serverLines).has(label), `server ${label}`);
  }
}

// ALPN.
{
  const { endpoint, client, server } = await pair(
    { cert, key, alpn: ['coap', 'h2'] }, { alpn: ['coap'] });
  assert.strictEqual(client.alpnProtocol, 'coap');
  assert.strictEqual(server.alpnProtocol, 'coap');
  await client.close();
  await endpoint.close();
}

// SNI, from a map and from a callback. The callback is only asked about a
// peer that has completed the cookie exchange, so once per handshake.
{
  const { endpoint, client, server } = await pair({
    cert, key,
    sni: { 'localhost': createSecureContext({ isServer: true, cert, key }) },
  }, { servername: 'localhost' });
  assert.strictEqual(server.servername, 'localhost');
  await client.close();
  await endpoint.close();
}
{
  const asked = [];
  const { endpoint, client, server } = await pair({
    cert, key,
    sni: (servername) => {
      asked.push(servername);
      return { cert, key };
    },
  }, { servername: 'localhost' });
  assert.strictEqual(server.servername, 'localhost');
  assert.deepStrictEqual(asked, ['localhost']);
  await client.close();
  await endpoint.close();
}

// PSK, from a map and from callbacks. DTLS 1.3 PSK authenticates without a
// certificate, and the session reports it as authorized. The server
// callback is asked for each ClientHello: the answer for the first steers
// the cipher suite the HelloRetryRequest commits to. The client's is too,
// since it builds a second ClientHello in answer to the HelloRetryRequest.
{
  const KEY = Buffer.from('0123456789abcdef0123456789abcdef', 'hex');
  for (const [serverPsk, clientPsk] of [
    [{ device: KEY }, { identity: 'device', key: KEY }],
    [mustCall((identity) => (identity === 'device' ? KEY : undefined), 2),
     mustCall(() => ({ identity: 'device', key: KEY }), 2)],
  ]) {
    const { endpoint, client, server } = await pair(
      { psk: serverPsk }, { psk: clientPsk, rejectUnauthorized: true });
    assert.strictEqual(client.authorized, true);
    assert.strictEqual(client.peerCertificate, undefined);
    assert.match(server.cipher.name, /SHA256$/);
    await client.close();
    await endpoint.close();
  }
}

// A client certificate.
{
  const { endpoint, client, server } = await pair(
    { cert, key, ca: [ca], requestCert: true }, { cert, key, ca: [ca] });
  assert.ok(server.peerCertificate);
  assert.strictEqual(server.authorized, true);
  await client.close();
  await endpoint.close();
}

// DTLS-SRTP keying.
{
  const profile = 'SRTP_AEAD_AES_128_GCM';
  const { endpoint, client, server } = await pair(
    { cert, key, srtp: profile }, { srtp: profile });
  assert.strictEqual(client.srtpProfile, profile);
  assert.strictEqual(server.srtpProfile, profile);
  const label = 'EXTRACTOR-dtls_srtp';
  assert.deepStrictEqual(client.exportKeyingMaterial(60, label),
                         server.exportKeyingMaterial(60, label));
  await client.close();
  await endpoint.close();
}

// The exporter is HKDF-Expand-Label, bounded at 255 hash lengths.
{
  const { endpoint, client } = await pair({ cert, key }, {});
  const hashLength = /SHA384$/.test(client.cipher.name) ? 48 : 32;
  const max = 255 * hashLength;
  assert.strictEqual(client.exportKeyingMaterial(max, 'EXPORTER-x').length,
                     max);
  assert.throws(() => client.exportKeyingMaterial(max + 1, 'EXPORTER-x'), {
    code: 'ERR_OUT_OF_RANGE',
    message: new RegExp(`<= ${max} under DTLS 1\\.3`),
  });
  await client.close();
  await endpoint.close();
}

// The largest send is one byte under the 2^14 record limit, which OpenSSL
// mis-measures on receipt under DTLS 1.3 and answers with record_overflow.
{
  const received = Promise.withResolvers();
  const { endpoint, client } = await pair({ cert, key }, {}, (session) => {
    session.onmessage = (data) => received.resolve(data.length);
  });
  assert.throws(() => client.send(Buffer.alloc(16384)), {
    code: 'ERR_OUT_OF_RANGE', message: /16383 byte maximum/,
  });
  assert.strictEqual(client.send(Buffer.alloc(16383)), 16383);
  assert.strictEqual(await received.promise, 16383);
  await client.close();
  await endpoint.close();
}

// Resumption. A DTLS 1.3 server issues its tickets after the handshake, so
// there is nothing to resume with when opened settles; onsessionticket
// reports each one as it arrives. Under DTLS 1.2 it reports the session the
// handshake established.
for (const versions of [V13, V12]) {
  const endpoint = listen(() => {}, {
    cert, key, host: HOST, port: 0, ...versions,
  });
  const first = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });
  // OpenSSL's server sends two tickets under DTLS 1.3 by default.
  const expected = versions === V13 ? 2 : 1;
  const tickets = [];
  const allTickets = Promise.withResolvers();
  first.onsessionticket = mustCall((t) => {
    tickets.push(t);
    if (tickets.length === expected) allTickets.resolve();
  }, expected);
  await first.opened;
  if (versions === V13) {
    assert.strictEqual(first.session, undefined);
  }
  await allTickets.promise;
  const session = tickets[0];
  assert.ok(Buffer.isBuffer(session));
  await first.close();

  const second = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, session, ...versions,
  });
  await second.opened;
  assert.strictEqual(second.reused, true);
  await second.close();
  await endpoint.close();
}

// A ticket earned without verification is not spent with it.
{
  const endpoint = listen(() => {}, { cert, key, host: HOST, port: 0, ...V13 });
  const first = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: false, ...V13,
  });
  const ticket = Promise.withResolvers();
  first.onsessionticket = (t) => ticket.resolve(t);
  await first.opened;
  assert.strictEqual(first.authorized, false);
  const session = await ticket.promise;
  await first.close();

  const second = connect(HOST, endpoint.address.port, {
    rejectUnauthorized: true, session, ...V13,
  });
  await assert.rejects(second.opened, /Peer certificate verification failed/);
  await endpoint.close();
}
