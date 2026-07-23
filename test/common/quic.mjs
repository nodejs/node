// Shared helpers for QUIC tests.
//
// Usage:
//   import { key, cert, listen, connect } from '../common/quic.mjs';
//
// Provides pre-loaded TLS credentials and thin wrappers around node:quic
// listen/connect that apply default options suitable for most tests.

import * as fixtures from '../common/fixtures.mjs';
import { setTimeout } from 'node:timers/promises';

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
    // Test helper defaults to 'manual' because tests use self-signed
    // certs without a CA. Tests that want to verify cert validation
    // behavior should set verifyPeer explicitly.
    verifyPeer = 'manual',
    ...rest
  } = options;
  return quic.connect(address, { alpn, verifyPeer, ...rest });
}

/**
 * Build a deterministic payload whose content depends on absolute position.
 *
 * Flow control bugs frequently show up as duplicated, dropped, or reordered
 * regions rather than as a wrong total length, so the pattern deliberately
 * varies over a long period (not a repeating 256-byte ramp) to make such
 * damage detectable by `hashBytes` below.
 * @param {number} size Number of bytes to generate.
 * @param {number} [seed] Offsets the pattern so callers can build distinct
 *   payloads of the same length.
 * @returns {Uint8Array}
 */
function makePayload(size, seed = 0) {
  const out = new Uint8Array(size);
  let state = (seed * 2654435761 + 1) >>> 0;
  for (let i = 0; i < size; i++) {
    // xorshift32 -- cheap, deterministic, and position sensitive.
    state ^= state << 13; state >>>= 0;
    state ^= state >>> 17;
    state ^= state << 5; state >>>= 0;
    out[i] = state & 0xff;
  }
  return out;
}

/**
 * Order-sensitive FNV-1a 32-bit hash.
 *
 * Note this is deliberately not a simple additive checksum: addition is
 * commutative, so it cannot distinguish correctly ordered data from
 * reordered data. Flow control errors can reorder or duplicate regions
 * while preserving the byte total, so verification needs to be sensitive to
 * position.
 * @param {Uint8Array} buf
 * @returns {number} Hash as an unsigned 32-bit integer.
 */
function hashBytes(buf) {
  let h = 0x811c9dc5;
  for (let i = 0; i < buf.byteLength; i++) {
    h ^= buf[i];
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h >>> 0;
}

/**
 * Write `size` bytes to the stream and wait until the peer has
 * acknowledged them.
 */
async function writeAndAwaitAck(stream, size) {
  stream.writer.write(new Uint8Array(size).fill(7));
  while (stream.stats.maxOffsetAcknowledged < BigInt(size)) await setTimeout(5);
}

/**
 * Send `size` bytes and then stall forever without a FIN
 * @yields {Uint8Array}
 */
async function* stallingBody(size) {
  yield new Uint8Array(size).fill(7);
  await new Promise(() => {});
}

/**
 * Do a full single stream-read scenario, with the given client options and
 * server body, and two hooks available to tweak behaviour, returning the
 * details of the result after completion.
 * @param {Function} serverBody
 * @param {object} [options]
 * @param {object} [options.clientOptions]  Options forwarded to connect().
 * @param {Function} [options.beforeIterate]
 * @param {Function} [options.onFirstChunk]
 * @returns {Promise<{received: number, threw: any, closedError: any}>}
 *   Bytes received, the iteration error if any, and the client stream's
 *   closed rejection if any.
 */
async function readStream(serverBody, options = {}) {
  const { clientOptions, beforeIterate, onFirstChunk } = options;

  const serverEndpoint = await listen((session) => {
    session.closed.catch(() => {});
    session.onstream = (stream) => {
      stream.closed.catch(() => {});
      serverBody(stream, session);
    };
  });

  const session = await connect(serverEndpoint.address, clientOptions);
  await session.opened;
  session.closed.catch(() => {});

  const stream = await session.createBidirectionalStream();
  await stream.writer.write(new Uint8Array([1]));

  let closedError;
  const closedSettled =
    stream.closed.then(() => {}, (err) => { closedError = err; });

  await beforeIterate?.({ stream, session });

  let received = 0;
  let threw;
  let firstChunk = true;
  try {
    for await (const chunk of stream) {
      for (const c of chunk) received += c.byteLength;
      if (firstChunk) {
        firstChunk = false;
        await onFirstChunk?.({ stream, session });
      }
    }
  } catch (err) {
    threw = err;
  }

  session.close();
  await closedSettled;
  await serverEndpoint.close();
  return { received, threw, closedError };
}

export {
  key,
  cert,
  listen,
  connect,
  makePayload,
  hashBytes,
  readStream,
  stallingBody,
  writeAndAwaitAck,
};
