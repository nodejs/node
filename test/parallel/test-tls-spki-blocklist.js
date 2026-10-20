// Flags: --expose-internals
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
// The fixtures give a chain of agent6 -> ca3 (intermediate) -> ca1 (root);
// agent6-cert.pem already bundles ca3. agent1 is signed directly by ca1 and
// serves as an unaffected server certificate for the client-certificate
// cases. The test blocks the SPKI of ca3 through the test-only hooks on the
// crypto binding.

const assert = require('assert');
const { createHash, X509Certificate } = require('crypto');
const { once } = require('events');
const { describe, it, beforeEach, afterEach } = require('node:test');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');

const {
  addBlockedSPKIForTesting,
  clearBlockedSPKIsForTesting,
} = internalBinding('crypto');

function spkiSha256(pem) {
  const spki = new X509Certificate(pem).publicKey.export({
    type: 'spki',
    format: 'der',
  });
  return createHash('sha256').update(spki).digest();
}

const rootCert = fixtures.readKey('ca1-cert.pem');
const intermediateCert = fixtures.readKey('ca3-cert.pem');
const leafKey = fixtures.readKey('agent6-key.pem');
const leafChain = fixtures.readKey('agent6-cert.pem');
const unaffectedKey = fixtures.readKey('agent1-key.pem');
const unaffectedCert = fixtures.readKey('agent1-cert.pem');
// ca2 is unrelated to the agent6 chain; appended to the server's certificate
// it is sent to the peer but never becomes part of the verified chain.
const unrelatedCert = fixtures.readKey('ca2-cert.pem');

const intermediateSpkiHash = spkiSha256(intermediateCert);
const leafSpkiHash = spkiSha256(leafChain);
const unrelatedSpkiHash = spkiSha256(unrelatedCert);

describe('SPKI blocklist on the server certificate', function() {
  let server;
  let client;
  let opts;

  beforeEach(async function() {
    server = tls.createServer({
      key: leafKey,
      cert: leafChain,
    }, (socket) => socket.resume());
    server.listen(0);
    await once(server, 'listening');

    opts = {
      port: server.address().port,
      ca: rootCert,
      checkServerIdentity() {},
    };
  });

  afterEach(async function() {
    clearBlockedSPKIsForTesting();
    client?.destroy();
    server?.close();
  });

  it('accepts the chain when nothing is blocked', async function() {
    client = tls.connect(opts);
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('rejects a chain through a blocked intermediate', async function() {
    addBlockedSPKIForTesting(intermediateSpkiHash);
    await assert.rejects(async () => {
      client = tls.connect(opts);
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('rejects a chain whose leaf key is blocked', async function() {
    addBlockedSPKIForTesting(leafSpkiHash);
    await assert.rejects(async () => {
      client = tls.connect(opts);
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('reports CERT_REVOKED without rejectUnauthorized', async function() {
    addBlockedSPKIForTesting(intermediateSpkiHash);
    client = tls.connect({ ...opts, rejectUnauthorized: false });
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, false);
    assert.strictEqual(client.authorizationError, 'CERT_REVOKED');
  });

  it('takes precedence over an untrusted-root error', async function() {
    // With an empty trust store the chain cannot reach a root, which would
    // normally be reported as UNABLE_TO_GET_ISSUER_CERT_LOCALLY. The
    // blocked key must win over that error.
    addBlockedSPKIForTesting(intermediateSpkiHash);
    await assert.rejects(async () => {
      client = tls.connect({ ...opts, ca: [] });
      await once(client, 'secureConnect');
    }, { code: 'CERT_REVOKED' });
  });

  it('accepts the chain again once the blocklist is cleared', async function() {
    addBlockedSPKIForTesting(intermediateSpkiHash);
    clearBlockedSPKIsForTesting();
    client = tls.connect(opts);
    await once(client, 'secureConnect');
    assert.strictEqual(client.authorized, true);
  });

  it('validates the hook arguments', function() {
    assert.throws(() => addBlockedSPKIForTesting('not a buffer'), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
    assert.throws(() => addBlockedSPKIForTesting(Buffer.alloc(31)), {
      code: 'ERR_OUT_OF_RANGE',
    });
  });
});

describe('SPKI blocklist on certificates outside the verified chain', function() {
  let server;
  let client;

  beforeEach(async function() {
    server = tls.createServer({
      key: leafKey,
      cert: `${leafChain}\n${unrelatedCert}`,
    }, (socket) => socket.resume());
    server.listen(0);
    await once(server, 'listening');
  });

  afterEach(async function() {
    clearBlockedSPKIsForTesting();
    client?.destroy();
    server?.close();
  });

  it('ignores an unused peer-sent certificate when the chain verifies', async function() {
    addBlockedSPKIForTesting(unrelatedSpkiHash);
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
    addBlockedSPKIForTesting(unrelatedSpkiHash);
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
      key: unaffectedKey,
      cert: unaffectedCert,
      ca: rootCert,
      requestCert: true,
      ...serverOpts,
    }, (socket) => socket.resume());
    server.listen(0);
    return once(server, 'listening');
  }

  function connect() {
    client = tls.connect({
      port: server.address().port,
      key: leafKey,
      cert: leafChain,
      ca: rootCert,
      checkServerIdentity() {},
    });
    return client;
  }

  afterEach(async function() {
    clearBlockedSPKIsForTesting();
    client?.destroy();
    server?.close();
  });

  it('accepts the client chain when nothing is blocked', async function() {
    await listen({ rejectUnauthorized: true });
    const connection = once(server, 'secureConnection');
    connect();
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, true);
  });

  it('reports CERT_REVOKED without rejectUnauthorized', async function() {
    addBlockedSPKIForTesting(intermediateSpkiHash);
    await listen({ rejectUnauthorized: false });
    const connection = once(server, 'secureConnection');
    connect();
    const [socket] = await connection;
    assert.strictEqual(socket.authorized, false);
    assert.strictEqual(socket.authorizationError, 'CERT_REVOKED');
  });

  it('rejects the client chain with rejectUnauthorized', async function() {
    addBlockedSPKIForTesting(intermediateSpkiHash);
    await listen({ rejectUnauthorized: true });
    server.on('secureConnection', common.mustNotCall());
    const clientError = once(server, 'tlsClientError');
    // The client may see either a clean close or a reset depending on
    // timing, so accept both and only wait for it to go away.
    const clientClosed = new Promise((resolve) => {
      connect().on('error', () => {}).once('close', resolve);
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
