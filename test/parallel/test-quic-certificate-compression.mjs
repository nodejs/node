// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const tls = await import('node:tls');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Certificate compression (RFC 8879) depends on the OpenSSL build linking in
// the compression libraries. Reuse the node:tls detection helper to decide
// whether the feature is available.
const supported = tls.getCertificateCompressionAlgorithms();
if (supported.length === 0) {
  skip('certificate compression not supported by this OpenSSL build');
}

// ---------------------------------------------------------------------------
// Option validation. The certificateCompression option is parsed by
// processTlsOptions during connect()/listen(), so invalid values reject the
// returned promise before any network activity happens.
// ---------------------------------------------------------------------------
{
  // Non-array values are rejected.
  await assert.rejects(
    connect('127.0.0.1:4433', { certificateCompression: true }),
    { code: 'ERR_INVALID_ARG_TYPE' });

  await assert.rejects(
    connect('127.0.0.1:4433', { certificateCompression: 'zlib' }),
    { code: 'ERR_INVALID_ARG_TYPE' });

  // Unknown algorithm names are rejected.
  await assert.rejects(
    connect('127.0.0.1:4433', { certificateCompression: ['invalid'] }),
    { code: 'ERR_INVALID_ARG_VALUE',
      message: /must be 'zlib', 'brotli', or 'zstd'/ });

  // Non-string entries are rejected.
  await assert.rejects(
    connect('127.0.0.1:4433', { certificateCompression: [1] }),
    { code: 'ERR_INVALID_ARG_VALUE',
      message: /must be 'zlib', 'brotli', or 'zstd'/ });

  // At most three algorithms may be specified.
  await assert.rejects(
    connect('127.0.0.1:4433', {
      certificateCompression: ['zlib', 'brotli', 'zstd', 'zlib'],
    }),
    { code: 'ERR_INVALID_ARG_VALUE', message: /at most 3 algorithms/ });
}

// ---------------------------------------------------------------------------
// Functional handshakes. Enabling certificate compression must not break the
// TLS 1.3 handshake. The on-the-wire size reduction is exercised by the
// node:tls certificate compression test; over QUIC the payload is encrypted
// and carried in UDP datagrams, so here we assert the handshake completes
// successfully with the option enabled on the server, the client, or both.
// ---------------------------------------------------------------------------
async function handshake({ serverAlgs, clientAlgs }) {
  const serverOpened = Promise.withResolvers();

  const endpoint = await listen(mustCall(async (serverSession) => {
    const info = await serverSession.opened;
    assert.strictEqual(info.protocol, 'h3');
    serverOpened.resolve();
    serverSession.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    ...(serverAlgs !== undefined ?
      { certificateCompression: serverAlgs } : {}),
  });

  const clientSession = await connect(endpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    ...(clientAlgs !== undefined ?
      { certificateCompression: clientAlgs } : {}),
  });

  const info = await clientSession.opened;
  assert.strictEqual(info.protocol, 'h3');

  await serverOpened.promise;
  await clientSession.close();
  await endpoint.close();
}

// Each supported algorithm negotiates a successful handshake when enabled on
// both peers.
for (const algo of supported) {
  await handshake({ serverAlgs: [algo], clientAlgs: [algo] });
}

// All supported algorithms advertised together.
await handshake({ serverAlgs: supported, clientAlgs: supported });

// Asymmetric configurations must also complete: a peer that does not advertise
// compression simply receives an uncompressed certificate.
await handshake({ serverAlgs: [supported[0]], clientAlgs: undefined });
await handshake({ serverAlgs: undefined, clientAlgs: [supported[0]] });

// An empty array is equivalent to disabling compression.
await handshake({ serverAlgs: [], clientAlgs: [] });
