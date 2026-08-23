// Flags: --experimental-dtls --no-warnings

// Test: session.peerX509Certificate.
//
// peerCertificate returns the leaf as PEM and nothing else, so the issuer
// chain and the parsed fields were unreachable without parsing the PEM by
// hand. This exposes crypto.X509Certificate instead of node:tls's legacy
// dictionary; toLegacyObject() is there for anyone who wants that shape.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { X509Certificate } from 'node:crypto';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

// A leaf issued by an intermediate, so there is a real chain to walk. The
// root that signed the intermediate is not in the fixtures, so verification
// is off throughout; what is under test is the chain's shape, not its trust.
const leafCert = fixtures.readKey('leaf-from-intermediate-cert.pem').toString();
const leafKey = fixtures.readKey('leaf-from-intermediate-key.pem').toString();
const intermediate = fixtures.readKey('intermediate-ca.pem').toString();
const chain = leafCert + intermediate;

const agentCert = fixtures.readKey('agent1-cert.pem').toString();
const agentKey = fixtures.readKey('agent1-key.pem').toString();

function commonNames(cert) {
  const names = [];
  for (let x = cert, i = 0; x && i < 8; i++) {
    names.push(String(x.subject).split('\n')
      .find((line) => line.startsWith('CN=')));
    const issuer = x.issuerCertificate;
    if (!issuer || issuer === x) break;
    x = issuer;
  }
  return names;
}

// The client sees the chain the server presented.
{
  const endpoint = listen(() => {}, {
    cert: chain, key: leafKey, host: '127.0.0.1', port: 0,
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;

  const cert = client.peerX509Certificate;

  assert.ok(cert instanceof X509Certificate);
  assert.deepStrictEqual(commonNames(cert),
                         ['CN=localhost', 'CN=NodeJS-Test-Intermediate-CA']);

  // The parsed fields that were the point of the exercise.
  assert.match(cert.subject, /CN=localhost/);
  assert.match(cert.issuer, /CN=NodeJS-Test-Intermediate-CA/);
  assert.match(cert.fingerprint256, /^([0-9A-F]{2}:){31}[0-9A-F]{2}$/);
  assert.ok(cert.validFromDate instanceof Date);
  assert.ok(cert.validToDate instanceof Date);
  assert.ok(Buffer.isBuffer(cert.raw));

  // node:tls's shape is still available for anyone who wants it.
  assert.strictEqual(cert.toLegacyObject().subject.CN, 'localhost');

  // Consistent with the PEM accessor, which keeps working either way round.
  assert.strictEqual(cert.toString(), client.peerCertificate);

  await client.close();
  await endpoint.close();
}

// The server sees the chain the client presented. This is the other branch
// of GetPeerCertificateFlag: SSL_get_peer_cert_chain() omits the peer's leaf
// on the server, so the wrong flag here silently drops or repeats it.
{
  const gotSession = Promise.withResolvers();

  const endpoint = listen((session) => gotSession.resolve(session), {
    cert: agentCert,
    key: agentKey,
    requestCert: true,
    rejectUnauthorized: false,
    host: '127.0.0.1',
    port: 0,
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    cert: chain, key: leafKey, rejectUnauthorized: false,
  });

  await client.opened;
  const session = await gotSession.promise;
  await session.opened;

  assert.deepStrictEqual(commonNames(session.peerX509Certificate),
                         ['CN=localhost', 'CN=NodeJS-Test-Intermediate-CA']);

  await client.close();
  await endpoint.close();
}

// Repeated reads return the same object.
//
// Not merely a convenience: X509Certificate::GetPeerCert() is destructive on
// the client side. Having no peer certificate of its own to start from, it
// lifts the leaf out of the SSL's chain with sk_X509_delete(), so calling it
// twice returns a shorter chain and then nothing. The accessor must call it
// once and keep the result.
{
  const endpoint = listen(() => {}, {
    cert: chain, key: leafKey, host: '127.0.0.1', port: 0,
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;

  const first = client.peerX509Certificate;
  const second = client.peerX509Certificate;

  assert.strictEqual(first, second);
  assert.strictEqual(first.fingerprint256, second.fingerprint256);
  assert.deepStrictEqual(commonNames(client.peerX509Certificate),
                         ['CN=localhost', 'CN=NodeJS-Test-Intermediate-CA']);

  await client.close();
  await endpoint.close();
}

// A peer that sent no certificate reports undefined rather than an empty one.
{
  const gotSession = Promise.withResolvers();

  const endpoint = listen((session) => gotSession.resolve(session), {
    cert: agentCert, key: agentKey, host: '127.0.0.1', port: 0,
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false,
  });

  await client.opened;
  const session = await gotSession.promise;
  await session.opened;

  assert.strictEqual(session.peerX509Certificate, undefined);
  assert.strictEqual(session.peerCertificate, undefined);

  await client.close();
  await endpoint.close();
}
