// Flags: --experimental-dtls --no-warnings

// Test: ALPN protocol lists are validated where they are supplied.
//
// The wire format is a sequence of one length byte followed by that many
// bytes. A name longer than 255 cannot be represented -- 256 truncated to a
// zero length byte and desynchronised the rest of the list -- and a
// pre-encoded Buffer was passed through with no checking at all. Neither was
// caught here; the first surfaced as an opaque ERR_CRYPTO_OPERATION_FAILED
// during the handshake, and a malformed Buffer just silently negotiated
// nothing.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const base = { cert, key, host: '127.0.0.1', port: 0 };

// A name that does not fit in the length byte.
assert.throws(
  () => listen(() => {}, { ...base, alpn: ['a'.repeat(256)] }),
  { code: 'ERR_OUT_OF_RANGE' });

// RFC 7301 gives ProtocolName a 1..255 length, so empty is not a valid entry.
assert.throws(
  () => listen(() => {}, { ...base, alpn: [''] }),
  { code: 'ERR_OUT_OF_RANGE' });

// The index of the offending entry is reported.
assert.throws(
  () => listen(() => {}, { ...base, alpn: ['h2', 'x'.repeat(300)] }),
  { code: 'ERR_OUT_OF_RANGE', message: /alpn\[1\]/ });

// 255 is the largest representable name and must still be accepted.
{
  const name = 'b'.repeat(255);
  const endpoint = listen(() => {}, { ...base, alpn: [name] });
  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca], alpn: [name],
  });
  await client.opened;
  assert.strictEqual(client.alpnProtocol, name);
  await client.close();
  await endpoint.close();
}

// Pre-encoded buffers are walked rather than trusted.
assert.throws(
  () => listen(() => {}, { ...base, alpn: Buffer.from([0, 0x68, 0x32]) }),
  { code: 'ERR_INVALID_ARG_VALUE', message: /zero-length/ });

assert.throws(
  () => listen(() => {}, { ...base, alpn: Buffer.from([9, 0x68, 0x32]) }),
  { code: 'ERR_INVALID_ARG_VALUE', message: /past the end/ });

// A well-formed buffer still works, so the walk is not rejecting everything.
{
  const endpoint = listen(() => {}, {
    ...base, alpn: Buffer.from([2, 0x68, 0x32]),
  });
  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca], alpn: ['h2'],
  });
  await client.opened;
  assert.strictEqual(client.alpnProtocol, 'h2');
  await client.close();
  await endpoint.close();
}

// Non-string entries are still rejected by type.
assert.throws(
  () => listen(() => {}, { ...base, alpn: [42] }),
  { code: 'ERR_INVALID_ARG_TYPE' });
