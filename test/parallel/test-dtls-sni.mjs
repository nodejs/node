// Flags: --experimental-dtls --no-warnings

// Test: server-side SNI, the `sni` option on listen().
//
// An endpoint could report session.servername but not act on it, so one port
// could only ever present one certificate. This follows the QUIC model: a
// declarative map of host name to identity, resolved in C++ during the
// handshake. Nothing calls into JavaScript mid-handshake, so the handshake is
// never suspended -- which matters more here than it does for TLS, because a
// suspended DTLS handshake keeps retransmitting.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');
const { connect, createSecureContext, listen } = dtls;

const key = (name) => fixtures.readKey(name).toString();

const agent1Cert = key('agent1-cert.pem');
const agent1Key = key('agent1-key.pem');
// A second identity with a different subject, to tell them apart.
const localhostCert = key('leaf-from-intermediate-cert.pem');
const localhostKey = key('leaf-from-intermediate-key.pem');

function servedCommonName(session) {
  return String(session.peerX509Certificate.subject)
    .split('\n').find((line) => line.startsWith('CN='));
}

// The name selects the certificate, and values may be either an options bag
// or a DTLSSecureContext.
{
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: {
      'agent1': { cert: agent1Cert, key: agent1Key },
      'localhost': createSecureContext({
        cert: localhostCert, key: localhostKey, isServer: true,
      }),
      '*': { cert: agent1Cert, key: agent1Key },
    },
  });

  for (const [servername, expected] of [
    ['agent1', 'CN=agent1'],
    ['localhost', 'CN=localhost'],
    ['no.such.host', 'CN=agent1'],   // Falls to the wildcard.
  ]) {
    const client = connect('127.0.0.1', endpoint.address.port, {
      servername, rejectUnauthorized: false,
    });
    await client.opened;
    assert.strictEqual(servedCommonName(client), expected);
    await client.close();
  }

  await endpoint.close();
}

// Without a wildcard, an unmatched name is refused rather than falling back
// to the endpoint's own certificate.
{
  // The session reaches onsession before SNI selection runs -- the server
  // builds it once DTLSv1_listen() has validated the cookie, and only then
  // drives the handshake that consults the map. So a refused name still
  // surfaces a session here, which then fails, exactly as any other
  // handshake failure does.
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    host: '127.0.0.1',
    port: 0,
    sni: { 'agent1': { cert: agent1Cert, key: agent1Key } },
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'not.configured', rejectUnauthorized: false,
  });

  await assert.rejects(client.opened, (err) => {
    assert.match(err.message, /unrecognized[ _]name/);
    return true;
  });

  await endpoint.close();
}

// A client that sends no SNI at all also lands on the wildcard, and is
// refused when there is none.
{
  const withWildcard = listen(() => {}, {
    cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0,
    sni: { '*': { cert: agent1Cert, key: agent1Key } },
  });

  // servername: '' disables the extension entirely.
  const ok = connect('127.0.0.1', withWildcard.address.port, {
    servername: '', rejectUnauthorized: false,
  });
  await ok.opened;
  assert.strictEqual(servedCommonName(ok), 'CN=agent1');
  await ok.close();
  await withWildcard.close();

  const withoutWildcard = listen(() => {}, {
    cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0,
    sni: { 'agent1': { cert: agent1Cert, key: agent1Key } },
  });

  const refused = connect('127.0.0.1', withoutWildcard.address.port, {
    servername: '', rejectUnauthorized: false,
  });
  await assert.rejects(refused.opened, { name: 'Error' });
  await withoutWildcard.close();
}

// Verification follows the selected identity. SSL_set_SSL_CTX() reassigns
// ssl->ctx and the verify store is read through it, so an identity that
// trusts only ca2 rejects a client presenting a ca1 certificate even though
// the endpoint itself trusts ca1.
{
  const endpoint = listen(() => {}, {
    cert: agent1Cert,
    key: agent1Key,
    ca: [key('ca1-cert.pem')],
    requestCert: true,
    rejectUnauthorized: true,
    host: '127.0.0.1',
    port: 0,
    sni: {
      '*': {
        cert: agent1Cert, key: agent1Key, ca: [key('ca1-cert.pem')],
      },
      'ca2-only': {
        cert: agent1Cert, key: agent1Key, ca: [key('ca2-cert.pem')],
      },
    },
  });

  // ca1 client against the ca1 identity: accepted.
  const accepted = connect('127.0.0.1', endpoint.address.port, {
    servername: 'anything',
    cert: agent1Cert, key: agent1Key,
    rejectUnauthorized: false,
  });
  await accepted.opened;
  await accepted.close();

  // The same client against the ca2-only identity: rejected.
  const rejected = connect('127.0.0.1', endpoint.address.port, {
    servername: 'ca2-only',
    cert: agent1Cert, key: agent1Key,
    rejectUnauthorized: false,
  });
  await assert.rejects(rejected.opened, { name: 'Error' });

  // A ca2 client against the ca2-only identity: accepted.
  const other = connect('127.0.0.1', endpoint.address.port, {
    servername: 'ca2-only',
    cert: key('agent3-cert.pem'), key: key('agent3-key.pem'),
    rejectUnauthorized: false,
  });
  await other.opened;
  await other.close();

  await endpoint.close();
}

// Validation.
{
  const base = { cert: agent1Cert, key: agent1Key, host: '127.0.0.1', port: 0 };

  assert.throws(() => listen(mustNotCall(), { ...base, sni: {} }),
                { code: 'ERR_INVALID_ARG_VALUE' });

  for (const sni of ['x', 42, null]) {
    assert.throws(() => listen(mustNotCall(), { ...base, sni }),
                  { code: 'ERR_INVALID_ARG_TYPE' });
  }

  for (const entry of ['x', 42, null]) {
    assert.throws(() => listen(mustNotCall(), {
      ...base, sni: { 'a.example': entry },
    }), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // A client context cannot serve as an identity.
  assert.throws(() => listen(mustNotCall(), {
    ...base,
    sni: { 'a.example': createSecureContext({ ca: [key('ca1-cert.pem')] }) },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}
