'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Every SSL_CTX rejects a certificate chain that contains a public key whose
// SubjectPublicKeyInfo SHA-256 hash is on the compiled-in blocklist, and
// reports the failure as CERT_REVOKED. The blocklist is checked against the
// verified chain, so a blocked intermediate is caught even though the root it
// chains to is fully trusted. Like any other verification error, the result
// is surfaced through socket.authorizationError and honours rejectUnauthorized
// on both the client and the server side.
//
// The public key of the blocked-ca fixture is a permanent entry on the
// blocklist. The fixtures provide two chains that contain it:
//   blocked-ca-leaf -> blocked-ca (intermediate) -> ca1 (root), and
//   blocked-key-leaf -> ca1, where the leaf reuses the blocked key itself.
// blocked-ca-leaf-cert.pem already bundles blocked-ca. The agent6 -> ca3 ->
// ca1 chain and agent1 (signed directly by ca1) are unaffected.

const assert = require('assert');
const { once } = require('events');
const { describe, it, beforeEach, afterEach } = require('node:test');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const rootCert = fixtures.readKey('ca1-cert.pem');
const blockedCaCert = fixtures.readKey('blocked-ca-cert.pem');
const blockedCaLeafKey = fixtures.readKey('blocked-ca-leaf-key.pem');
const blockedCaLeafChain = fixtures.readKey('blocked-ca-leaf-cert.pem');
const blockedKey = fixtures.readKey('blocked-ca-key.pem');
const blockedKeyLeafCert = fixtures.readKey('blocked-key-leaf-cert.pem');
const unaffectedKey = fixtures.readKey('agent6-key.pem');
const unaffectedChain = fixtures.readKey('agent6-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const serverCert = fixtures.readKey('agent1-cert.pem');

describe('SPKI blocklist on the server certificate', function() {
  let server;
  let client;

  function listen(serverOpts) {
    server = tls.createServer(serverOpts, (socket) => socket.resume());
    server.listen(0);
    return once(server, 'listening');
  }

  function connect(clientOpts) {
    client = tls.connect({
      port: server.address().port,
      ca: rootCert,
      checkServerIdentity() {},
      ...clientOpts,
    });
    return client;
  }

  afterEach(function() {
    client?.destroy();
    server?.close();
  });

  it('accepts a chain that contains no blocked key', async function() {
    await listen({ key: unaffectedKey, cert: unaffectedChain });
    connect();
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('rejects a chain through a blocked intermediate', async function() {
    await listen({ key: blockedCaLeafKey, cert: blockedCaLeafChain });
    await assert.rejects(async () => {
      connect();
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('rejects a chain whose leaf key is blocked', async function() {
    await listen({ key: blockedKey, cert: blockedKeyLeafCert });
    await assert.rejects(async () => {
      connect();
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('reports CERT_REVOKED without rejectUnauthorized', async function() {
    await listen({ key: blockedCaLeafKey, cert: blockedCaLeafChain });
    connect({ rejectUnauthorized: false });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, false);
    assert.strictEqual(client.authorizationError, 'CERT_REVOKED');
  });

  it('takes precedence over an untrusted-root error', async function() {
    // With an empty trust store the chain cannot reach a root, which would
    // normally be reported as UNABLE_TO_GET_ISSUER_CERT_LOCALLY. The
    // blocked key must win over that error.
    await listen({ key: blockedCaLeafKey, cert: blockedCaLeafChain });
    await assert.rejects(async () => {
      connect({ ca: [] });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });
});

describe('SPKI blocklist on certificates outside the verified chain', function() {
  let server;
  let client;

  // blocked-ca is unrelated to the agent6 chain; appended to the server's
  // certificate it is sent to the peer but never becomes part of the
  // verified chain.
  beforeEach(async function() {
    server = tls.createServer({
      key: unaffectedKey,
      cert: `${unaffectedChain}\n${blockedCaCert}`,
    }, (socket) => socket.resume());
    server.listen(0);
    await once(server, 'listening');
  });

  afterEach(function() {
    client?.destroy();
    server?.close();
  });

  it('ignores an unused peer-sent certificate when the chain verifies', async function() {
    client = tls.connect({
      port: server.address().port,
      ca: rootCert,
      checkServerIdentity() {},
    });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('checks every peer-sent certificate when the chain does not verify', async function() {
    // With an empty trust store the chain is incomplete, so the fallback
    // scan of everything the peer sent applies and finds the blocked key.
    await assert.rejects(async () => {
      client = tls.connect({
        port: server.address().port,
        ca: [],
        checkServerIdentity() {},
      });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });
});

describe('SPKI blocklist on the client certificate', function() {
  let server;
  let client;

  function listen(serverOpts) {
    server = tls.createServer({
      key: serverKey,
      cert: serverCert,
      ca: rootCert,
      requestCert: true,
      ...serverOpts,
    }, (socket) => socket.resume());
    server.listen(0);
    return once(server, 'listening');
  }

  function connect(clientOpts) {
    client = tls.connect({
      port: server.address().port,
      ca: rootCert,
      checkServerIdentity() {},
      ...clientOpts,
    });
    return client;
  }

  afterEach(function() {
    client?.destroy();
    server?.close();
  });

  it('accepts a client chain that contains no blocked key', async function() {
    await listen({ rejectUnauthorized: true });
    const connection = once(server, 'secureConnection');
    connect({ key: unaffectedKey, cert: unaffectedChain });
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, true);
  });

  it('reports CERT_REVOKED without rejectUnauthorized', async function() {
    await listen({ rejectUnauthorized: false });
    const connection = once(server, 'secureConnection');
    connect({ key: blockedCaLeafKey, cert: blockedCaLeafChain });
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, false);
    assert.strictEqual(socket.authorizationError, 'CERT_REVOKED');
  });

  it('rejects the client chain with rejectUnauthorized', async function() {
    await listen({ rejectUnauthorized: true });
    server.on('secureConnection', common.mustNotCall());
    const clientError = once(server, 'tlsClientError');
    // The client may see either a clean close or a reset depending on
    // timing, so accept both and only wait for it to go away.
    const clientClosed = new Promise((resolve) => {
      connect({ key: blockedCaLeafKey, cert: blockedCaLeafChain })
        .on('error', () => {}).once('close', resolve);
    });
    // The server destroys the socket during 'secure', which surfaces as a
    // reset rather than a handshake failure; the reason is kept on the socket.
    const [err, socket] = await clientError;
    assert.strictEqual(err.code, 'ECONNRESET');
    assert.strictEqual(socket.authorized, false);
    assert.strictEqual(socket.authorizationError, 'CERT_REVOKED');
    await clientClosed;
  });
});
