// Flags: --experimental-quic --no-warnings

// Test: custom TLS ciphers and groups.
// Custom ciphers option on the server/client.
// Custom groups option on the server/client.
// Default ciphers/groups used when not specified.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, ok } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, constants } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// Custom ciphers. Use a specific TLS 1.3 cipher suite.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    const info = await serverSession.opened;
    strictEqual(typeof info.cipher, 'string');
    ok(info.cipher.includes('AES_256_GCM'));
    serverSession.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
    ciphers: 'TLS_AES_256_GCM_SHA384',
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    ciphers: 'TLS_AES_256_GCM_SHA384',
  });

  const info = await clientSession.opened;
  ok(info.cipher.includes('AES_256_GCM'));
  strictEqual(info.cipherVersion, 'TLSv1.3');

  await clientSession.closed;
  await serverEndpoint.close();
}

// Custom groups. Use a specific key exchange group.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    serverSession.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    alpn: ['quic-test'],
    groups: 'P-256',
  });

  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    groups: 'P-256',
  });

  const info = await clientSession.opened;
  // The handshake should succeed with the specified group.
  assert.strictEqual(info.cipherVersion, 'TLSv1.3');

  await clientSession.closed;
  await serverEndpoint.close();
}

// Default ciphers/groups are non-empty strings from constants.
{
  strictEqual(typeof constants.DEFAULT_CIPHERS, 'string');
  ok(constants.DEFAULT_CIPHERS.length > 0);
  strictEqual(typeof constants.DEFAULT_GROUPS, 'string');
  ok(constants.DEFAULT_GROUPS.length > 0);
}
