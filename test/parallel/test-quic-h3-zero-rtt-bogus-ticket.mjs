// Flags: --experimental-quic --no-warnings

// Test: Bogus session ticket data is rejected gracefully.
// Providing random bytes as a session ticket throws ERR_INVALID_ARG_VALUE
// because the ticket format is validated before use. The connection
// cannot proceed with garbage ticket data.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey, randomBytes } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const serverEndpoint = await listen(mustNotCall(), {
  sni: { '*': { keys: [key], certs: [cert] } },
});

// Bogus ticket data (random bytes) is rejected at the format level.
await rejects(
  connect(serverEndpoint.address, {
    servername: 'localhost',
    sessionTicket: randomBytes(256),
  }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

serverEndpoint.close();
