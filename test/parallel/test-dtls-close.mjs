// Flags: --experimental-dtls --no-warnings

// Test: Graceful close (close_notify) and forced destroy.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = fixtures.readKey('agent1-cert.pem');
const serverKey = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

// Test 1: Graceful close from client side.
{
  const serverSessionClosed = Promise.withResolvers();

  const endpoint = listen(mustCall((session) => {
    session.onmessage = mustNotCall();
    session.closed.then(mustCall(() => {
      serverSessionClosed.resolve();
    }));
  }), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
  });

  const session = connect('127.0.0.1', endpoint.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
  });

  await session.opened;

  // Graceful close.
  const closedPromise = session.close();
  assert.ok(closedPromise instanceof Promise);
  await closedPromise;

  // Wait for server to see the close.
  await serverSessionClosed.promise;

  endpoint.close();
  await endpoint.closed;
}

// Test 2: Forced destroy.
{
  const endpoint = listen(mustCall((session) => {
    session.onmessage = mustNotCall();
  }), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
  });

  const session = connect('127.0.0.1', endpoint.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
  });

  await session.opened;

  // Forced destroy - no close_notify.
  session.destroy();

  // After destroy, send should fail.
  assert.throws(() => {
    session.send('should fail');
  }, /destroyed/i);

  endpoint.destroy();
}

// Test 3: Endpoint close closes all sessions.
{
  const endpoint = listen(mustCall((session) => {
    session.onmessage = mustNotCall();
  }), {
    cert: serverCert.toString(),
    key: serverKey.toString(),
    port: 0,
    host: '127.0.0.1',
  });

  const session = connect('127.0.0.1', endpoint.address.port, {
    ca: [ca.toString()],
    rejectUnauthorized: false,
  });

  await session.opened;

  // Close the endpoint - this should close all sessions.
  await endpoint.close();
}
