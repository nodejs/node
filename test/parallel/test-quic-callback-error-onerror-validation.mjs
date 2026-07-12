// Flags: --experimental-quic --no-warnings

// Test: onerror setter validation.
// Setting onerror to a non-function (including null) throws.
// Setting to undefined clears it. Setting to a function works.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const errorCheck = {
  code: 'ERR_INVALID_ARG_TYPE',
};

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  // Session onerror validation: non-functions throw.
  assert.throws(() => { serverSession.onerror = 'not a function'; }, errorCheck);
  assert.throws(() => { serverSession.onerror = 42; }, errorCheck);
  assert.throws(() => { serverSession.onerror = null; }, errorCheck);

  // Setting to a function works.
  const fn = () => {};
  serverSession.onerror = fn;
  // The getter returns the bound version, not the original.
  assert.strictEqual(typeof serverSession.onerror, 'function');

  // Setting to undefined clears it.
  serverSession.onerror = undefined;
  assert.strictEqual(serverSession.onerror, undefined);

  serverSession.close();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Client-side stream onerror validation.
const stream = await clientSession.createBidirectionalStream({
  body: new TextEncoder().encode('x'),
});

assert.throws(() => { stream.onerror = 'not a function'; }, errorCheck);
assert.throws(() => { stream.onerror = 42; }, errorCheck);
assert.throws(() => { stream.onerror = null; }, errorCheck);

// Setting to a function works.
stream.onerror = () => {};
assert.strictEqual(typeof stream.onerror, 'function');

// Setting to undefined clears it.
stream.onerror = undefined;
assert.strictEqual(stream.onerror, undefined);

await clientSession.closed;
await serverEndpoint.close();
