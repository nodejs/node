// Flags: --experimental-dtls --no-warnings

// Test: the requestCert / rejectUnauthorized matrix on a DTLS server.
//
//   requestCert: false            -> no certificate is requested
//   requestCert, rejectUnauthorized -> OpenSSL enforces, peer gets an alert
//   requestCert, !rejectUnauthorized -> certificate is requested, the
//                                       handshake completes either way, and
//                                       the application decides using
//                                       session.authorized
//
// The last row is the node:tls idiom for "ask for a certificate and let the
// application decide". It previously mapped to SSL_VERIFY_NONE, so no
// CertificateRequest was sent at all and the server saw no peer certificate
// even when the client offered a valid one.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca1 = fixtures.readKey('ca1-cert.pem');
const ca2 = fixtures.readKey('ca2-cert.pem');

// Runs one handshake and reports what the server made of the client.
async function handshake(serverOptions, clientOptions) {
  const gotSession = Promise.withResolvers();
  const server = listen((session) => {
    session.onhandshake = () => gotSession.resolve(session);
  }, { cert, key, host: '127.0.0.1', port: 0, ...serverOptions });

  const client = connect('127.0.0.1', server.address.port, {
    servername: 'agent1', ca: [ca1], ...clientOptions,
  });

  let result;
  try {
    await client.opened;
    const session = await gotSession.promise;
    result = {
      rejected: false,
      sawPeerCertificate: session.peerCertificate !== undefined,
      authorized: session.authorized,
      authorizationError: session.authorizationError,
    };
  } catch (err) {
    result = { rejected: true, message: err.message };
  }

  try {
    await client.close();
  } catch {
    // The handshake may already have torn the session down.
  }
  await server.close();
  return result;
}

// --- requestCert with rejectUnauthorized disabled ------------------------

// A trusted client certificate is actually requested and received. This is
// the case that silently produced no certificate at all before.
assert.deepStrictEqual(
  await handshake({ requestCert: true, rejectUnauthorized: false, ca: [ca1] },
                  { cert, key }),
  {
    rejected: false,
    sawPeerCertificate: true,
    authorized: true,
    authorizationError: undefined,
  });

// An untrusted client certificate is received, and the handshake completes so
// the application can decide.
assert.deepStrictEqual(
  await handshake({ requestCert: true, rejectUnauthorized: false, ca: [ca2] },
                  { cert, key }),
  {
    rejected: false,
    sawPeerCertificate: true,
    authorized: false,
    authorizationError: 'SELF_SIGNED_CERT_IN_CHAIN',
  });

// A client that declines to send one is tolerated rather than rejected.
assert.deepStrictEqual(
  await handshake({ requestCert: true, rejectUnauthorized: false, ca: [ca1] },
                  {}),
  {
    rejected: false,
    sawPeerCertificate: false,
    authorized: false,
    authorizationError: 'UNABLE_TO_GET_ISSUER_CERT',
  });

// --- requestCert, enforced -----------------------------------------------

// OpenSSL rejects these, so the peer receives a real alert rather than having
// the session quietly dropped after the handshake.
{
  const noCert = await handshake({ requestCert: true, ca: [ca1] }, {});
  assert.strictEqual(noCert.rejected, true);
  assert.match(noCert.message, /handshake failure/);

  const untrusted = await handshake({ requestCert: true, ca: [ca2] },
                                    { cert, key });
  assert.strictEqual(untrusted.rejected, true);
  assert.match(untrusted.message, /unknown ca/);
}

// --- no requestCert ------------------------------------------------------

// rejectUnauthorized alone must not turn a server into one that demands
// client certificates.
assert.deepStrictEqual(
  await handshake({ rejectUnauthorized: true, ca: [ca1] }, { cert, key }),
  {
    rejected: false,
    sawPeerCertificate: false,
    authorized: false,
    authorizationError: 'UNABLE_TO_GET_ISSUER_CERT',
  });

// --- validation ----------------------------------------------------------

assert.throws(
  () => listen(mustNotCall(), {
    cert, key, host: '127.0.0.1', port: 0, requestCert: 'yes',
  }),
  { code: 'ERR_INVALID_ARG_TYPE' });
