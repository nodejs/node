// Flags: --experimental-quic --no-warnings

// Test: a readable truncated by the connection idle timeout delivers the
// data it received, then ends per the truncatedReads policy: an error under
// the default (so an incomplete stream can never look complete), and a clean
// end under 'allow' (an idle timeout carries no error).

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { readStream, stallingBody } = await import('../common/quic.mjs');

// The server sends 1000 bytes then stalls without a FIN, so the short (1s)
// connection idle timeout is the only thing that ends the read side.
const serve = (stream) => { stream.setBody(stallingBody(1000)); };
const transportParams = { maxIdleTimeout: 1 };

{
  const { received, threw } =
    await readStream(serve, { clientOptions: { transportParams } });
  strictEqual(received, 1000);
  strictEqual(threw?.code, 'ERR_QUIC_STREAM_ABORTED');
  strictEqual(threw.errorCode, 0n);
}
{
  const { received, threw } = await readStream(serve, {
    clientOptions: { transportParams, truncatedReads: 'allow' },
  });
  strictEqual(received, 1000);
  strictEqual(threw, undefined);
}
