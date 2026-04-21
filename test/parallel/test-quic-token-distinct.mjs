// Flags: --experimental-quic --no-warnings

// Test: two clients receive distinct NEW_TOKEN tokens.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, notDeepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

let token1;
let token2;
const gotToken1 = Promise.withResolvers();
const gotToken2 = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}, 2));

// Client 1.
const cs1 = await connect(serverEndpoint.address, {
  onnewtoken: mustCall((token) => {
    token1 = Buffer.from(token);
    gotToken1.resolve();
  }),
});
await Promise.all([cs1.opened, gotToken1.promise]);
ok(token1.length > 0);

// Client 2.
const cs2 = await connect(serverEndpoint.address, {
  onnewtoken: mustCall((token) => {
    token2 = Buffer.from(token);
    gotToken2.resolve();
  }),
});
await Promise.all([cs2.opened, gotToken2.promise]);
ok(token2.length > 0);

// Tokens should be distinct.
notDeepStrictEqual(token1, token2);

await cs1.close();
await cs2.close();
await serverEndpoint.close();
