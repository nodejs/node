// Flags: --experimental-quic --no-warnings --expose-internals

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, notStrictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// Test that the h3 ALPN negotiates normally but doesn't auto-activate
// HTTP/3. ALPN is reported, not type-changing. HTTP/3 is used via
// the 'node:http3' module explicitly.
// Both server and client explicitly configure the h3 ALPN.

const { createRequire } = await import('node:module');
const require = createRequire(import.meta.url);
const { getQuicSessionState } = require('internal/quic/quic');

const serverOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // ALPN negotiated h3, but with no application requested this is a RAW
  // session: the h3 application is not installed (headersSupported ===
  // 2, i.e. unsupported).
  strictEqual(serverSession.alpnProtocol, 'h3');
  strictEqual(getQuicSessionState(serverSession).headersSupported, 2);
  const info = await serverSession.opened;
  strictEqual(info.protocol, 'h3');
  serverOpened.resolve();
  serverSession.close();
}), {
  alpn: ['h3'],
  sni: { '*': { keys: [key], certs: [cert] } },
});

notStrictEqual(serverEndpoint.address, undefined);

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'h3',
  servername: 'localhost',
  verifyPeer: 'manual',
  // Application config is internal-only (kApplication) - user options are ignored
  // Applications integrate natively with their consumers so custom options
  // would reliably break things.
  applicationName: 'http3',
});

async function checkClient() {
  const info = await clientSession.opened;
  strictEqual(info.protocol, 'h3');
  // Still a raw session:
  strictEqual(getQuicSessionState(clientSession).headersSupported, 2);
}

await Promise.all([serverOpened.promise, checkClient()]);
await clientSession.close();
await serverEndpoint.close();
