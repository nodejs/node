// Shared QUIC test helpers for session-level setup and teardown.
// Complements test-client.mjs and test-server.mjs (ngtcp2 binary wrappers).

import { hasQuic, skip, mustCall } from '../index.mjs';
import * as fixtures from '../fixtures.mjs';

/**
 * Guard check. Skips the test if QUIC is not available.
 * Call at the top of every QUIC test after imports.
 */
export function checkQuic() {
  if (!hasQuic) {
    skip('QUIC is not enabled');
  }
}

/**
 * Returns TLS credentials from test fixtures.
 * @returns {{ keys: KeyObject, certs: Buffer }}
 */
export async function defaultCerts() {
  const { createPrivateKey } = await import('node:crypto');
  const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
  const certs = fixtures.readKey('agent1-cert.pem');
  return { keys, certs };
}

/**
 * Creates a connected client-server QUIC pair.
 * Returns the endpoint, both sessions, and a cleanup function.
 * @param {object} [options]
 * @param {object} [options.serverOptions] - Additional options for listen().
 * @param {object} [options.clientOptions] - Additional options for connect().
 * @returns {Promise<{
 *   endpoint: QuicEndpoint,
 *   serverSession: QuicSession,
 *   clientSession: QuicSession,
 *   cleanup: () => Promise<void>
 * }>}
 */
export async function createQuicPair(options = {}) {
  const { listen, connect } = await import('node:quic');
  const { keys, certs } = await defaultCerts();

  const serverReady = Promise.withResolvers();

  const endpoint = await listen(mustCall((session) => {
    serverReady.resolve(session);
  }), { keys, certs, ...options.serverOptions });

  const clientSession = await connect(endpoint.address, options.clientOptions);

  // Wait for both sides to complete the handshake.
  const [serverSession] = await Promise.all([
    serverReady.promise,
    clientSession.opened,
  ]);
  await serverSession.opened;

  async function cleanup() {
    clientSession.close();
    serverSession.close();
    await Promise.allSettled([clientSession.closed, serverSession.closed]);
    await endpoint.close();
  }

  return { endpoint, serverSession, clientSession, cleanup };
}
