// Flags: --experimental-quic --no-warnings

// Test: onerror setter validation.
// Setting onerror to a non-function (including null) throws.
// Setting to undefined clears it. Setting to a function works.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, throws } = assert;

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
  throws(() => { serverSession.onerror = 'not a function'; }, errorCheck);
  throws(() => { serverSession.onerror = 42; }, errorCheck);
  throws(() => { serverSession.onerror = null; }, errorCheck);

  // Setting to a function works.
  const fn = () => {};
  serverSession.onerror = fn;
  // The getter returns the bound version, not the original.
  strictEqual(typeof serverSession.onerror, 'function');

  // Setting to undefined clears it.
  serverSession.onerror = undefined;
  strictEqual(serverSession.onerror, undefined);

  serverSession.close();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Client-side stream onerror validation.
const stream = await clientSession.createBidirectionalStream({
  body: new TextEncoder().encode('x'),
});

throws(() => { stream.onerror = 'not a function'; }, errorCheck);
throws(() => { stream.onerror = 42; }, errorCheck);
throws(() => { stream.onerror = null; }, errorCheck);

// Setting to a function works.
stream.onerror = () => {};
strictEqual(typeof stream.onerror, 'function');

// Setting to undefined clears it.
stream.onerror = undefined;
strictEqual(stream.onerror, undefined);

await clientSession.closed;
