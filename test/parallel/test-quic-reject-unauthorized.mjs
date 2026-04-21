// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { strictEqual, ok, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// rejectUnauthorized must be a boolean
await rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  rejectUnauthorized: 'yes',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// With rejectUnauthorized: true (the default), connecting with self-signed
// certs and no CA should produce a validation error in the handshake info.

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
  await serverSession.closed;
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
  // Default: rejectUnauthorized: true
});

const info = await clientSession.opened;
// Self-signed cert without CA should produce a validation error.
strictEqual(typeof info.validationErrorReason, 'string');
ok(info.validationErrorReason.length > 0);
strictEqual(typeof info.validationErrorCode, 'string');
ok(info.validationErrorCode.length > 0);

await clientSession.close();
await serverEndpoint.close();
