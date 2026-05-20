// Flags: --experimental-quic --no-warnings

// Test: transport parameter validation.
// Transport parameters are validated by ngtcp2 at connection time.
// Node.js validates the type (must be a number) but ngtcp2 validates
// the ranges. Verify that invalid types are rejected and valid
// values are accepted.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { readKey } = fixtures;

const { rejects, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];

async function tryListen(sessionOpts) {
  return listen(mustNotCall(), { sni, alpn, ...sessionOpts });
}

// Invalid types for transport params are rejected.
for (const param of [
  'initialMaxStreamDataBidiLocal',
  'initialMaxStreamDataBidiRemote',
  'initialMaxStreamDataUni',
  'initialMaxData',
  'initialMaxStreamsBidi',
  'initialMaxStreamsUni',
  'maxIdleTimeout',
  'activeConnectionIDLimit',
  'ackDelayExponent',
  'maxAckDelay',
]) {
  await rejects(tryListen({
    transportParams: { [param]: 'invalid' },
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  }, `${param} should reject string value`);

  await rejects(tryListen({
    transportParams: { [param]: -1 },
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  }, `${param} should reject negative value`);
}

// Valid values are accepted.
const ep = await tryListen({
  transportParams: {
    initialMaxStreamDataBidiLocal: 65536,
    initialMaxStreamDataBidiRemote: 65536,
    initialMaxStreamDataUni: 65536,
    initialMaxData: 1048576,
    initialMaxStreamsBidi: 100,
    initialMaxStreamsUni: 3,
    maxIdleTimeout: 30,
    activeConnectionIDLimit: 4,
    ackDelayExponent: 3,
    maxAckDelay: 25,
    maxDatagramFrameSize: 1200,
  },
});
ok(ep);
await ep.close();
