// Flags: --experimental-quic --no-warnings

// Test: string datagram with multi-byte UTF-8 characters.
// Verifies that sendDatagram with a string containing multi-byte UTF-8
// characters (CJK, emoji, etc.) encodes correctly and the receiver
// gets the exact bytes.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const message = '\u4f60\u597d\u4e16\u754c';  // "Hello World" in Chinese
const expected = Buffer.from(message, 'utf8');

const serverGot = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverGot.promise;
  await serverSession.close();
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall((data) => {
    ok(data instanceof Uint8Array);
    // Verify the received bytes match the UTF-8 encoding.
    deepStrictEqual(Buffer.from(data), expected);
    serverGot.resolve();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

const id = await clientSession.sendDatagram(message);
strictEqual(id, 1n);

await Promise.all([serverGot.promise, clientSession.closed]);
await serverEndpoint.close();
