// Shared helpers for QUIC tests.
//
// Usage:
//   import { key, cert, listen, connect } from '../common/quic.mjs';
//
// Provides pre-loaded TLS credentials and thin wrappers around node:quic
// listen/connect that apply default options suitable for most tests.

import * as fixtures from '../common/fixtures.mjs';

const { createPrivateKey } = await import('node:crypto');
const quic = await import('node:quic');

// Pre-loaded TLS credentials from the standard agent1 fixture pair.
const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

/**
 * Start a QUIC server with sensible test defaults.
 * @param {Function} callback  The session callback (receives QuicSession).
 * @param {object} [options]   Options forwarded to quic.listen(). The
 *   following defaults are applied when not specified:
 *     - sni: { '*': { keys: [key], certs: [cert] } }
 *     - alpn: ['quic-test']
 * @returns {Promise<QuicEndpoint>}
 */
async function listen(callback, options = {}) {
  const {
    sni = { '*': { keys: [key], certs: [cert] } },
    alpn = ['quic-test'],
    ...rest
  } = options;
  return quic.listen(callback, { sni, alpn, ...rest });
}

/**
 * Connect a QUIC client with sensible test defaults.
 * @param {SocketAddress|string} address  The server address.
 * @param {object} [options]  Options forwarded to quic.connect(). The
 *   following defaults are applied when not specified:
 *     - alpn: 'quic-test'
 * @returns {Promise<QuicSession>}
 */
async function connect(address, options = {}) {
  const {
    alpn = 'quic-test',
    ...rest
  } = options;
  return quic.connect(address, { alpn, ...rest });
}

export {
  key,
  cert,
  listen,
  connect,
};
