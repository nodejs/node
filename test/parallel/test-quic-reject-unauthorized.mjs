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

// verifyPeer must be one of 'strict', 'auto', 'manual'
await rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  verifyPeer: 'invalid',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

const serverEndpoint = await listen(async (serverSession) => {
  serverSession.onerror = () => {};
  await rejects(serverSession.opened, (err) => {
    ok(err.code === 'ERR_QUIC_TRANSPORT_ERROR' ||
        err.code === 'ERR_INVALID_STATE');
    return true;
  });
  serverSession.close();
}, {
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

// --- verifyPeer: 'manual' (current behavior) ---
// The session.opened promise resolves even with an invalid cert.
// The validation error is available in the info object.
{
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    verifyPeer: 'manual',
  });

  const info = await clientSession.opened;
  // Self-signed cert without CA should produce a validation error.
  strictEqual(typeof info.validationErrorReason, 'string');
  ok(info.validationErrorReason.length > 0);
  strictEqual(typeof info.validationErrorCode, 'string');
  ok(info.validationErrorCode.length > 0);

  await clientSession.close();
}

// --- verifyPeer: 'auto' (default) ---
// The session.opened promise rejects when the cert is invalid.
{
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    // verifyPeer defaults to 'auto'
  });

  await rejects(clientSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}

// --- verifyPeer: 'strict' ---
// The TLS handshake itself fails. The session.opened promise rejects.
{
  const clientSession = await connect(serverEndpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    verifyPeer: 'strict',
    // The TLS failure triggers onerror before opened rejects.
    onerror: mustCall((err) => {
      ok(err, 'strict mode should fire onerror on invalid cert');
    }),
  });

  await rejects(clientSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}

await serverEndpoint.close();
