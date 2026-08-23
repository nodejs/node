// Flags: --experimental-dtls --no-warnings

// Test: pre-shared keys (RFC 4279).
//
// PSK is the usual way DTLS is deployed to constrained devices, which often
// have no certificate at all. RFC 7252 makes TLS_PSK_WITH_AES_128_CCM_8
// mandatory to implement for CoAP.

import { hasCrypto, mustCall, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');
const { connect, listen } = dtls;

const KEY = Buffer.from('0123456789abcdef');
const OTHER = Buffer.from('fedcba9876543210');
const HOST = '127.0.0.1';

const readKey = (name) => fixtures.readKey(name).toString();

function open(endpoint, options) {
  return connect(HOST, endpoint.address.port, options);
}

// A map on the server, an identity on the client. No certificate anywhere.
{
  const endpoint = listen(mustCall(), {
    psk: { 'device-42': KEY }, host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'device-42', key: KEY } });
  await client.opened;

  // A PSK suite must actually have been negotiated -- the default cipher
  // list excludes PSK, so this also pins the implicit enabling below.
  assert.match(client.cipher.standardName, /_PSK_/);
  assert.strictEqual(client.authorized, true);

  await client.close();
  await endpoint.close();
}

// A callback on either side, for keys that are looked up or derived rather
// than known up front.
{
  const endpoint = listen(() => {}, {
    psk: mustCall((identity) => {
      assert.strictEqual(identity, 'device-42');
      return KEY;
    }),
    host: HOST, port: 0,
  });

  const client = open(endpoint, {
    psk: mustCall(() => ({ identity: 'device-42', key: KEY })),
  });
  await client.opened;
  assert.match(client.cipher.standardName, /_PSK_/);

  await client.close();
  await endpoint.close();
}

// The map answers first; the callback is only reached on a miss. This is what
// keeps a map-only configuration from running JavaScript inside the
// handshake at all.
{
  const endpoint = listen(() => {}, {
    psk: { 'in-map': KEY }, host: HOST, port: 0,
  });
  const client = open(endpoint, { psk: { identity: 'in-map', key: KEY } });
  await client.opened;
  await client.close();
  await endpoint.close();
}

// An identity the server does not know fails closed rather than negotiating
// with an empty key.
{
  const endpoint = listen(() => {}, {
    psk: { 'known': KEY }, host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'unknown', key: KEY } });
  await assert.rejects(client.opened, { name: 'Error' });

  await endpoint.close();
}

// The right identity with the wrong key does not fail -- it stalls.
//
// The identity only names the key, so the handshake proceeds and the two
// sides derive different secrets. The first record that fails authentication
// is then silently discarded rather than answered with an alert, because
// DTLS discards invalid records instead of replying to them (RFC 6347
// section 4.1.2.1). Neither peer is told anything, and both retransmit.
//
// Asserted rather than fixed: it is the protocol's behaviour, not this
// implementation's. Note that nothing here times out, so the session lives
// until the endpoint is closed.
{
  const endpoint = listen(() => {}, {
    psk: { 'device-42': KEY }, host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'device-42', key: OTHER } });

  const stalled = Symbol('stalled');
  const outcome = await Promise.race([
    client.opened.then(() => 'opened', () => 'rejected'),
    new Promise((resolve) => setTimeout(resolve, 200, stalled)),
  ]);
  assert.strictEqual(outcome, stalled);

  client.destroy();
  await endpoint.close();
}

// A callback returning nothing is a refusal, not an error.
{
  const endpoint = listen(() => {}, {
    psk: mustCall(() => undefined), host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'anyone', key: KEY } });
  await assert.rejects(client.opened, { name: 'Error' });

  await endpoint.close();
}

// The cookie exchange still runs, so PSK is not a way around the address
// validation the endpoint's DoS protection depends on.
{
  const endpoint = listen(() => {}, {
    psk: { 'device-42': KEY }, host: HOST, port: 0,
  });

  const before = Number(endpoint.stats.packetsReceived);
  const client = open(endpoint, { psk: { identity: 'device-42', key: KEY } });
  await client.opened;
  const received = Number(endpoint.stats.packetsReceived) - before;

  assert.ok(received >= 2,
            `PSK handshake cost the server ${received} packets; fewer than ` +
            '2 means the cookie exchange was skipped');

  await client.close();
  await endpoint.close();
}

// An explicit cipher list wins, including the suite CoAP requires.
//
// TLS_PSK_WITH_AES_128_CCM_8 authenticates with a 64-bit tag, which OpenSSL
// refuses at security level 1 and above -- and Node's default is above it.
// Asking for it therefore needs @SECLEVEL=0, and asking without one does not
// report anything: the handshake stalls, because a DTLS peer with nothing to
// select discards rather than replies. Anyone implementing CoAP will meet
// this, so it is pinned here.
{
  const ciphers = 'PSK-AES128-CCM8@SECLEVEL=0';
  const endpoint = listen(() => {}, {
    psk: { 'device-42': KEY }, ciphers, host: HOST, port: 0,
  });

  const client = open(endpoint, {
    psk: { identity: 'device-42', key: KEY }, ciphers,
  });
  await client.opened;
  assert.strictEqual(client.cipher.standardName,
                     'TLS_PSK_WITH_AES_128_CCM_8');

  await client.close();
  await endpoint.close();
}

// Without an explicit list a forward-secret suite is chosen. Plain PSK
// derives its keys from the shared secret alone, so anyone who later learns
// the key can decrypt recorded traffic.
{
  const endpoint = listen(() => {}, {
    psk: { 'device-42': KEY }, host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'device-42', key: KEY } });
  await client.opened;
  assert.match(client.cipher.standardName, /_(EC)?DHE_PSK_/);

  await client.close();
  await endpoint.close();
}

// A certificate still works alongside PSK, because DEFAULT stays on the end
// of the implicit cipher list.
{
  const endpoint = listen(() => {}, {
    cert: readKey('agent1-cert.pem'),
    key: readKey('agent1-key.pem'),
    psk: { 'device-42': KEY },
    host: HOST, port: 0,
  });

  const psk = open(endpoint, { psk: { identity: 'device-42', key: KEY } });
  await psk.opened;
  assert.match(psk.cipher.standardName, /_PSK_/);
  await psk.close();

  const cert = open(endpoint, { rejectUnauthorized: false });
  await cert.opened;
  assert.doesNotMatch(cert.cipher.standardName, /_PSK_/);
  await cert.close();

  await endpoint.close();
}

// The identity hint is a server-side courtesy (RFC 4279 section 5.2) and
// reaches the client's callback.
{
  const endpoint = listen(() => {}, {
    psk: { 'device-42': KEY },
    pskIdentityHint: 'example.com',
    host: HOST, port: 0,
  });

  const client = open(endpoint, {
    psk: mustCall((hint) => {
      assert.strictEqual(hint, 'example.com');
      return { identity: 'device-42', key: KEY };
    }),
  });
  await client.opened;

  await client.close();
  await endpoint.close();
}

// A callback that throws fails that handshake and reports the error on the
// session. It must not reach the process as an uncaughtException: one bad
// callback should not take a server down.
{
  process.on('uncaughtException', mustNotCall());

  const thrown = new Error('from the psk callback');
  const seen = Promise.withResolvers();

  const endpoint = listen((session) => {
    session.onerror = (err) => seen.resolve(err);
  }, {
    psk: () => { throw thrown; },
    host: HOST, port: 0,
  });

  const client = open(endpoint, { psk: { identity: 'device-42', key: KEY } });
  await assert.rejects(client.opened, { name: 'Error' });

  // The user's own error, not something wrapped around it.
  assert.strictEqual(await seen.promise, thrown);

  await endpoint.close();
  process.removeAllListeners('uncaughtException');
}

// Validation.
{
  const port = 0;

  // A server map must have entries, and they must be non-empty buffers.
  assert.throws(() => listen(() => {}, { psk: {}, host: HOST, port }),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => listen(() => {}, {
    psk: { 'a': Buffer.alloc(0) }, host: HOST, port,
  }), { code: 'ERR_INVALID_ARG_VALUE' });
  for (const value of ['string', 42, null]) {
    assert.throws(() => listen(() => {}, {
      psk: { 'a': value }, host: HOST, port,
    }), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // A client needs both halves.
  assert.throws(() => connect(HOST, 1234, { psk: { key: KEY } }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => connect(HOST, 1234, { psk: { identity: 'a' } }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => connect(HOST, 1234, {
    psk: { identity: 'a', key: Buffer.alloc(0) },
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // The hint is a server-side notion.
  assert.throws(() => connect(HOST, 1234, {
    psk: { identity: 'a', key: KEY }, pskIdentityHint: 'nope',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  for (const value of [42, 'string', null]) {
    assert.throws(() => listen(() => {}, { psk: value, host: HOST, port }),
                  { code: /^ERR_INVALID_ARG_(TYPE|VALUE)$/ });
  }
}

// A PSK-only server needs no certificate, but one without either is still an
// error rather than a server that can never complete a handshake.
{
  assert.throws(() => listen(() => {}, { host: HOST, port: 0 }),
                { code: 'ERR_MISSING_ARGS' });
}
