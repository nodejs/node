// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a readable stream truncated by the connection idle timeout delivers
// the data it received, then surfaces the truncation as an error at the end of
// iteration - rather than a silent clean end-of-stream that would make an
// incomplete stream look complete.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// The body sends 1000 bytes then hangs (never a FIN), so the only thing that
// ends the client's read side is the connection idle timeout.
async function* stallingBody() {
  yield new Uint8Array(1000).fill(7);
  await new Promise(() => {});
}

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    stream.setBody(stallingBody());
    stream.closed.catch(() => {});
  });
}));

const clientSession = await connect(serverEndpoint.address, {
  // Short connection idle timeout so the truncation happens quickly.
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
await stream.writer.write(new Uint8Array([1]));
stream.closed.catch(() => {});

let received = 0;
let threw;
try {
  for await (const chunk of stream) {
    for (const c of chunk) received += c.byteLength;
  }
} catch (err) {
  threw = err;
}

// All the buffered data was delivered before the error.
strictEqual(received, 1000);
// The truncation surfaced as an error at the end, not a clean end-of-stream.
strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');

await serverEndpoint.close();
