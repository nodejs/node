'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Server-side client-certificate authorization must survive TLS session
// resumption. On a resumed handshake the client does not re-send its
// certificate, so the server has to report the same authorization state it
// derived from the original full handshake:
//
//   - a trusted certificate stays authorized,
//   - an untrusted certificate stays unauthorized with its verification error,
//   - a missing certificate stays unauthorized (UNABLE_TO_GET_ISSUER_CERT).
//
// The missing-certificate case is special on TLS 1.3: ncrypto reports X509_V_OK
// for the resumed PSK handshake even though no certificate was presented, so
// the absence has to be detected explicitly (see onServerSocketSecure() in
// lib/internal/tls/wrap.js). The final case checks that such a certificate-less
// resumed session is rejected outright when rejectUnauthorized is set.

const assert = require('assert');
const crypto = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { once } = require('events');

const ca = fixtures.readKey('ca1-cert.pem');
const serverCert = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};

// Client certificate variants, keyed by the peer state they produce.
const CLIENTS = {
  trusted: {                                          // Signed by ca1
    creds: {
      key: fixtures.readKey('agent1-key.pem'),
      cert: fixtures.readKey('agent1-cert.pem'),
    },
    authorized: true,
    authorizationError: null,
    peerCN: 'agent1',
  },
  untrusted: {                                        // Signed by ca2, not trusted
    creds: {
      key: fixtures.readKey('agent3-key.pem'),
      cert: fixtures.readKey('agent3-cert.pem'),
    },
    authorized: false,
    authorizationError: 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
    peerCN: 'agent3',
  },
  missing: {                                          // No client certificate
    creds: {},
    authorized: false,
    authorizationError: 'UNABLE_TO_GET_ISSUER_CERT',
    peerCN: undefined,
  },
};

async function handshake(options, captureSession) {
  const socket = tls.connect(options);
  const sessionPromise = captureSession ?
    once(socket, 'session').then(([session]) => session) : null;

  socket.resume();
  await once(socket, 'secureConnect');

  const closePromise = once(socket, 'close');
  const session = sessionPromise ? await sessionPromise : undefined;
  socket.end();
  await closePromise;
  return session;
}

// Test a single resumption configuration and expected result:
async function testResumption(version, name) {
  const { creds, authorized, authorizationError, peerCN } = CLIENTS[name];

  let connections = 0;
  const server = tls.createServer({
    ...serverCert,
    ca,
    requestCert: true,
    rejectUnauthorized: false,
    minVersion: version,
    maxVersion: version,
  }, common.mustCall((socket) => {
    // 2nd conn must resume:
    const resumed = connections++ === 1;
    const where = `${version} ${name} ${resumed ? 'resumed' : 'new'}`;
    assert.strictEqual(socket.isSessionReused(), resumed, where);

    // Both conns must report same expected auth state:
    assert.strictEqual(socket.authorized, authorized, where);
    assert.strictEqual(socket.authorizationError, authorizationError, where);
    const peer = socket.getPeerCertificate();
    if (peerCN === undefined)
      assert.deepStrictEqual(peer, {}, where);
    else
      assert.strictEqual(peer.subject.CN, peerCN, where);

    socket.resume();
  }, 2));

  server.listen(0);
  await once(server, 'listening');

  const options = {
    port: server.address().port,
    host: '127.0.0.1',
    checkServerIdentity: () => undefined,
    rejectUnauthorized: false,
    minVersion: version,
    maxVersion: version,
    ...creds,
  };

  try {
    const session = await handshake(options, true);
    assert(session);
    await handshake({ ...options, session });
  } finally {
    server.close();
    await once(server, 'close');
  }
}

// Test the special case of resumption from rejectUnauthorized:false to
// rejectUnauthorized:true, which must be rejected even though the original
// session worked initially.
async function testRejectResumedWithoutCert() {
  const options = {
    ...serverCert,
    ca,
    requestCert: true,
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
    ticketKeys: crypto.randomBytes(48),
  };
  const lenient = tls.createServer({ ...options, rejectUnauthorized: false });
  lenient.on('secureConnection', common.mustCall((socket) => {
    assert.strictEqual(socket.authorized, false);
    assert.strictEqual(socket.authorizationError, 'UNABLE_TO_GET_ISSUER_CERT');
    socket.resume();
  }));

  const strict = tls.createServer({ ...options, rejectUnauthorized: true });
  strict.on('secureConnection', common.mustNotCall());

  const clientOptions = (port) => ({
    port,
    host: '127.0.0.1',
    rejectUnauthorized: false,
    checkServerIdentity: () => undefined,
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
  });

  lenient.listen(0);
  await once(lenient, 'listening');
  const session = await handshake(clientOptions(lenient.address().port), true);
  assert(session);
  lenient.close();
  await once(lenient, 'close');

  strict.listen(0);
  await once(strict, 'listening');

  const resumed = tls.connect({ ...clientOptions(strict.address().port), session });
  resumed.on('error', () => {});  // May observe the server's reset.
  resumed.resume();

  // The client completes the resumed handshake (it has the server's Finished)
  // before the server's reset can arrive, so this asserts the strict server
  // actually resumed rather than falling back to a rejected full handshake.
  await once(resumed, 'secureConnect');
  assert.strictEqual(resumed.isSessionReused(), true);

  // Then the socket is destroyed during 'secure', which surfaces as a reset
  // rather than a handshake failure.
  const [err] = await once(strict, 'tlsClientError');
  assert.strictEqual(err.code, 'ECONNRESET');

  resumed.destroy();
  strict.close();
  await once(strict, 'close');
}

(async function() {
  // Run the full matrix of configurations:
  for (const version of ['TLSv1.2', 'TLSv1.3'])
    for (const name of Object.keys(CLIENTS))
      await testResumption(version, name);

  // Validate the rejectUnauth:false->true case
  await testRejectResumedWithoutCert();
})().then(common.mustCall());
