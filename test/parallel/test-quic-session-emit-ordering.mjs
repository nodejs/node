// Flags: --experimental-quic --no-warnings --expose-internals

// Check that server `onsession` emit fires only after the session's
// ClientHello has been processed & validated.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createRequire } = await import('node:module');
const require = createRequire(import.meta.url);
const { getQuicSessionState } = require('internal/quic/quic');
const { listen, connect } = await import('../common/quic.mjs');

const sessionSeen = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  // All assertions run synchronously in the onsession emit frame.

  // The TLS details from the ClientHello are readable on the session.
  assert.strictEqual(serverSession.servername, 'localhost');
  assert.strictEqual(serverSession.alpnProtocol, 'quic-test');

  // The client's transport params arrived in the first flight and have
  // been processed by the time the session is surfaced.
  const params = serverSession.remoteTransportParams;
  assert.notStrictEqual(params, undefined);
  assert.notStrictEqual(params, null);
  assert.ok(params.initialMaxStreamsBidi >= 0n);

  // ALPN negotiation completed, but no application has been installed yet
  // (type 0): the window to attach one is still open in this frame.
  assert.strictEqual(getQuicSessionState(serverSession).applicationType, 0);

  sessionSeen.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;
await sessionSeen.promise;

await clientSession.close();
await serverEndpoint.close();
