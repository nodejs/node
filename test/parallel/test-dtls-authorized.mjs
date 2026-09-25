// Flags: --experimental-dtls --no-warnings

// Test: session.authorized and session.authorizationError report the result of
// peer certificate chain verification.
//
// OpenSSL verifies the chain even under SSL_VERIFY_NONE -- it just does not
// abort on failure -- so these stay accurate when rejectUnauthorized is false,
// which is what makes a custom authorization policy possible.

import { hasCrypto, mustCall, skip } from '../common/index.mjs';
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

// --- Client side ---------------------------------------------------------

const endpoint = listen(() => {}, {
  cert, key, host: '127.0.0.1', port: 0,
});
const { port } = endpoint.address;

for (const [description, options, authorized, authorizationError] of [
  ['a trusted chain and matching identity verifies',
   { servername: 'agent1', ca: [ca1] },
   true, undefined],
  ['an untrusted chain reports the issuer failure',
   { servername: 'agent1', ca: [ca2], rejectUnauthorized: false },
   false, 'UNABLE_TO_GET_ISSUER_CERT_LOCALLY'],
  ['a trusted chain with the wrong identity reports the name mismatch',
   { servername: 'not-agent1', ca: [ca1], rejectUnauthorized: false },
   false, 'HOSTNAME_MISMATCH'],
]) {
  const client = connect('127.0.0.1', port, options);
  await client.opened;
  assert.strictEqual(client.authorized, authorized, description);
  assert.strictEqual(client.authorizationError, authorizationError,
                     description);
  await client.close();
}

await endpoint.close();

// --- Server side ---------------------------------------------------------

// A peer that sent no certificate is reported as unverified rather than
// authorized, even though OpenSSL has nothing to find fault with.
{
  const serverSession = Promise.withResolvers();
  const server = listen(mustCall((session) => {
    session.onhandshake = mustCall(() => serverSession.resolve(session));
  }), { cert, key, host: '127.0.0.1', port: 0 });

  const client = connect('127.0.0.1', server.address.port, {
    servername: 'agent1', ca: [ca1],
  });
  await client.opened;

  const session = await serverSession.promise;
  assert.strictEqual(session.authorized, false);
  assert.strictEqual(session.authorizationError, 'UNABLE_TO_GET_ISSUER_CERT');

  await client.close();
  await server.close();
}

// A peer that sent a chain the server trusts is authorized.
{
  const serverSession = Promise.withResolvers();
  const server = listen(mustCall((session) => {
    session.onhandshake = mustCall(() => serverSession.resolve(session));
  }), {
    cert, key, ca: [ca1], requestCert: true, host: '127.0.0.1', port: 0,
  });

  const client = connect('127.0.0.1', server.address.port, {
    cert, key, servername: 'agent1', ca: [ca1],
  });
  await client.opened;

  const session = await serverSession.promise;
  assert.strictEqual(session.authorized, true);
  assert.strictEqual(session.authorizationError, undefined);

  await client.close();
  await server.close();
}
