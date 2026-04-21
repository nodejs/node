// Flags: --experimental-quic --no-warnings

// Test: maxDatagramFrameSize transport param validation.
// The maxDatagramFrameSize transport parameter must be a uint16
// (0-65535). Values outside this range or of the wrong type should
// be rejected.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

// Invalid values for maxDatagramFrameSize — must be rejected.
const invalid = [
  -1,
  65536,
  1.5,
  'a',
  null,
  false,
  true,
  {},
  [],
  () => {},
];

for (const maxDatagramFrameSize of invalid) {
  const transportParams = { maxDatagramFrameSize };
  await rejects(
    listen(mustNotCall(), { sni, alpn, transportParams }),
    { code: 'ERR_INVALID_ARG_VALUE' },
    `listen should reject maxDatagramFrameSize: ${maxDatagramFrameSize}`,
  );
}

// Valid values — should not throw.
const valid = [0, 1, 100, 1200, 65535];

for (const maxDatagramFrameSize of valid) {
  const transportParams = { maxDatagramFrameSize };
  const ep = await listen(mustNotCall(), { sni, alpn, transportParams });
  ep.close();
  await ep.closed;
}
