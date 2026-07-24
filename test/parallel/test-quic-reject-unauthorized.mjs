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

// verifyPeer must be one of 'strict', 'auto', 'manual'
await assert.rejects(connect({ port: 1234 }, {
  alpn: 'quic-test',
  verifyPeer: 'invalid',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onerror = () => {};
  return assert.rejects(serverSession.opened, (err) =>
    err.code === 'ERR_QUIC_TRANSPORT_ERROR' ||
        err.code === 'ERR_INVALID_STATE',
  ).then(mustCall(() => {
    serverSession.close();
  }));
}), {
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
  assert.strictEqual(typeof info.validationErrorReason, 'string');
  assert.ok(info.validationErrorReason.length > 0);
  assert.strictEqual(typeof info.validationErrorCode, 'string');
  assert.ok(info.validationErrorCode.length > 0);

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

  await assert.rejects(clientSession.opened, {
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
      assert.ok(err, 'strict mode should fire onerror on invalid cert');
    }),
  });

  await assert.rejects(clientSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}

await serverEndpoint.close();
