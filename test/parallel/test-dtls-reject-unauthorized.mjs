// Flags: --experimental-dtls --no-warnings

// Test: rejectUnauthorized gates certificate-chain verification. A certificate
// that does not chain to a trusted CA is rejected when rejectUnauthorized is
// true and accepted when it is false.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { match, rejects } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();

// Case 1: with rejectUnauthorized, a certificate that does not chain to a
// trusted CA fails verification. The servername matches the certificate, so
// the failure is the untrusted chain and not an identity mismatch. No `ca` is
// supplied, so only the system-default CAs are trusted (which do not include
// this test's CA).
{
  const server = listen(mustCall(), {
    cert, key, port: 0, host: '127.0.0.1',
  });

  const client = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: true,
    servername: 'agent1',
  });

  await rejects(client.opened, /certificate verify failed/i);

  await client.endpoint.closed;
  await server.close();
}

// Case 2: the same untrusted certificate is accepted when verification is off.
{
  const server = listen(mustCall(), {
    cert, key, port: 0, host: '127.0.0.1',
  });

  const client = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: false,
  });

  const { protocol } = await client.opened;
  match(protocol, /DTLS/i);

  await client.close();
  await server.close();
}
