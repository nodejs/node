// Flags: --experimental-quic --no-warnings

// Application-level no-error codes are interpreted in the context of the
// negotiated application protocol.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import { createPrivateKey } from 'node:crypto';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// H3_NO_ERROR (0x100) and unknown HTTP/3 error codes are clean closes.
// 0x21 is one of the reserved codes defined by RFC 9114 Section 8.1.
for (const code of [0x100n, 0x21n]) {
  const serverSessionReady = Promise.withResolvers();
  const endpoint = await listen(mustCall((session) => {
    serverSessionReady.resolve(session);
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(endpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    reuseEndpoint: false,
  });
  const serverSession = await serverSessionReady.promise;
  serverSession.onerror = mustNotCall();
  await Promise.all([
    clientSession.opened,
    serverSession.opened,
  ]);

  await clientSession.close({
    code,
    type: 'application',
    reason: 'normal HTTP/3 close',
  });
  await serverSession.closed;
  await endpoint.close();
}

// H3_DATAGRAM_ERROR (0x33) is a known HTTP/3 application error.
{
  const serverSessionReady = Promise.withResolvers();
  const endpoint = await listen(mustCall((session) => {
    serverSessionReady.resolve(session);
  }), {
    alpn: ['h3'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(endpoint.address, {
    alpn: 'h3',
    servername: 'localhost',
    verifyPeer: 'manual',
    reuseEndpoint: false,
  });
  const serverSession = await serverSessionReady.promise;
  await Promise.all([
    clientSession.opened,
    serverSession.opened,
  ]);

  const closed = assert.rejects(serverSession.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
    errorCode: 0x33n,
    reason: 'HTTP datagram failure',
  });
  await clientSession.close({
    code: 0x33n,
    type: 'application',
    reason: 'HTTP datagram failure',
  });
  await closed;
  await endpoint.close();
}

// The same application code is not implicitly clean for another ALPN.
{
  const serverSessionReady = Promise.withResolvers();
  const endpoint = await listen(mustCall((session) => {
    serverSessionReady.resolve(session);
  }), {
    alpn: ['quic-test'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(endpoint.address, {
    alpn: 'quic-test',
    servername: 'localhost',
    verifyPeer: 'manual',
    reuseEndpoint: false,
  });
  const serverSession = await serverSessionReady.promise;
  await Promise.all([
    clientSession.opened,
    serverSession.opened,
  ]);

  const closed = assert.rejects(serverSession.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
    errorCode: 0x100n,
    reason: 'application failure',
  });
  await clientSession.close({
    code: 0x100n,
    type: 'application',
    reason: 'application failure',
  });
  await closed;
  await endpoint.close();
}
