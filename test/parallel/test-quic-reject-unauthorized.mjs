// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// rejectUnauthorized must be a boolean
await assert.rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  rejectUnauthorized: 'yes',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// With rejectUnauthorized: true (the default), connecting with self-signed
// certs and no CA should produce a validation error in the handshake info.

const serverOpened = Promise.withResolvers();
const clientOpened = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.opened.then(mustCall((info) => {
    serverOpened.resolve();
    serverSession.close();
  }));
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
  // Default: rejectUnauthorized: true
});
clientSession.opened.then(mustCall((info) => {
  // Self-signed cert without CA should produce a validation error.
  assert.strictEqual(typeof info.validationErrorReason, 'string');
  assert.ok(info.validationErrorReason.length > 0);
  assert.strictEqual(typeof info.validationErrorCode, 'string');
  assert.ok(info.validationErrorCode.length > 0);
  clientOpened.resolve();
}));

await Promise.all([serverOpened.promise, clientOpened.promise]);
clientSession.close();
