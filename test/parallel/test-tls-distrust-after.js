'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A client rejects a server certificate chain through a root on the
// compiled-in distrust-after list when the leaf certificate's notBefore is
// after that root's cutoff, and reports the failure as CERT_REVOKED.
// Certificates issued up to the cutoff keep verifying. Like any other
// verification error, the result is surfaced through socket.authorizationError
// and honours rejectUnauthorized. The list mirrors Mozilla's
// CKA_NSS_SERVER_DISTRUST_AFTER, a TLS server authentication policy, so a
// server verifying client certificates does not apply it (same scope as the
// 2016 StartCom/WoSign cutoff).
//
// The distrust-after-ca fixture is a permanent entry on the list with a
// cutoff of 2019-12-31 23:59:59 UTC. The fixtures provide two leaves it
// issued: distrust-after-pre-leaf with notBefore exactly at the cutoff second
// (the last accepted instant) and distrust-after-post-leaf with notBefore one
// second after it. distrust-after-cross is the same CA key cross-signed by
// ca1, so leaf -> cross -> ca1 is a chain that carries the date-limited key
// as an intermediate rather than as the trust anchor. The agent6 -> ca3 -> ca1
// chain and agent1 (signed by ca1) are unaffected.

const assert = require('assert');
const { once } = require('events');
const { describe, it, afterEach } = require('node:test');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const rootCert = fixtures.readKey('ca1-cert.pem');
const distrustedCaKey = fixtures.readKey('distrust-after-ca-key.pem');
const distrustedCaCert = fixtures.readKey('distrust-after-ca-cert.pem');
const crossCert = fixtures.readKey('distrust-after-cross-cert.pem');
const preLeafKey = fixtures.readKey('distrust-after-pre-leaf-key.pem');
const preLeafCert = fixtures.readKey('distrust-after-pre-leaf-cert.pem');
const postLeafKey = fixtures.readKey('distrust-after-post-leaf-key.pem');
const postLeafCert = fixtures.readKey('distrust-after-post-leaf-cert.pem');
const unaffectedKey = fixtures.readKey('agent6-key.pem');
const unaffectedChain = fixtures.readKey('agent6-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const serverCert = fixtures.readKey('agent1-cert.pem');

describe('distrust-after on the server certificate', function() {
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
      ca: distrustedCaCert,
      checkServerIdentity() {},
      ...clientOpts,
    });
    return client;
  }

  afterEach(function() {
    client?.destroy();
    server?.close();
  });

  it('accepts a chain that contains no date-limited root', async function() {
    await listen({ key: unaffectedKey, cert: unaffectedChain });
    connect({ ca: rootCert });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('accepts a leaf issued exactly at the cutoff', async function() {
    await listen({ key: preLeafKey, cert: preLeafCert });
    connect();
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('rejects a leaf issued after the cutoff', async function() {
    await listen({ key: postLeafKey, cert: postLeafCert });
    await assert.rejects(async () => {
      connect();
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('reports CERT_REVOKED without rejectUnauthorized', async function() {
    await listen({ key: postLeafKey, cert: postLeafCert });
    connect({ rejectUnauthorized: false });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, false);
    assert.strictEqual(client.authorizationError, 'CERT_REVOKED');
  });

  it('takes precedence over an untrusted-root error', async function() {
    // With the date-limited root sent by the peer but absent from the trust
    // store, the chain cannot reach a root, which would normally be reported
    // as SELF_SIGNED_CERT_IN_CHAIN. The cutoff must win over that error.
    await listen({ key: postLeafKey, cert: `${postLeafCert}\n${distrustedCaCert}` });
    await assert.rejects(async () => {
      connect({ ca: [] });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('matches the key when it is a cross-signed intermediate', async function() {
    // leaf -> cross (date-limited key, signed by ca1) -> ca1. The list is
    // keyed by public key, not by certificate, so the cutoff still applies
    // even though the self-signed root is nowhere in the chain.
    await listen({ key: postLeafKey, cert: `${postLeafCert}\n${crossCert}` });
    await assert.rejects(async () => {
      connect({ ca: rootCert });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('accepts a pre-cutoff leaf through the cross-signed intermediate', async function() {
    await listen({ key: preLeafKey, cert: `${preLeafCert}\n${crossCert}` });
    connect({ ca: rootCert });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('rejects the date-limited certificate itself as the leaf', async function() {
    // The CA certificate used directly as a server certificate: the
    // date-limited key is at depth 0 and its own notBefore (when the fixture
    // was generated) is after the cutoff. Trusted and untrusted alike.
    await listen({ key: distrustedCaKey, cert: distrustedCaCert });
    await assert.rejects(async () => {
      connect();
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
    client.destroy();
    await assert.rejects(async () => {
      connect({ ca: [] });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('applies to https', async function() {
    server = https.createServer({ key: postLeafKey, cert: postLeafCert });
    server.listen(0);
    await once(server, 'listening');
    await assert.rejects(async () => {
      const req = https.get({
        port: server.address().port,
        ca: distrustedCaCert,
        checkServerIdentity() {},
      });
      client = req;
      await once(req, 'response');
    }, { code: 'CERT_REVOKED' });
  });
});

describe('distrust-after does not apply to client certificates', function() {
  let server;
  let client;

  function listen(serverOpts) {
    server = tls.createServer({
      key: serverKey,
      cert: serverCert,
      ca: distrustedCaCert,
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

  it('accepts a client leaf issued exactly at the cutoff', async function() {
    await listen({ rejectUnauthorized: true });
    const connection = once(server, 'secureConnection');
    connect({ key: preLeafKey, cert: preLeafCert });
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, true);
  });

  it('accepts a client leaf issued after the cutoff', async function() {
    // The server-authentication cutoff is not a statement about client
    // authentication, so the same leaf a client would reject as a server
    // certificate is a valid client certificate.
    await listen({ rejectUnauthorized: true });
    server.on('tlsClientError', common.mustNotCall());
    const connection = once(server, 'secureConnection');
    connect({ key: postLeafKey, cert: postLeafCert });
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, true);
    assert.strictEqual(socket.authorizationError, null);
  });

  it('still blocks a client leaf issued after the cutoff by a blocked key', async function() {
    // Sanity check of the scoping: the SPKI blocklist is not date-based and
    // keeps applying to client certificates.
    await listen({
      ca: fixtures.readKey('blocked-ca-cert.pem'),
      rejectUnauthorized: false,
    });
    const connection = once(server, 'secureConnection');
    connect({
      key: fixtures.readKey('blocked-ca-leaf-key.pem'),
      cert: fixtures.readKey('blocked-ca-leaf-cert.pem'),
    });
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, false);
    assert.strictEqual(socket.authorizationError, 'CERT_REVOKED');
  });
});
