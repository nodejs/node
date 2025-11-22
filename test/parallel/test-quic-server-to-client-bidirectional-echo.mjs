// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { ok, strictEqual } from 'node:assert';
import { readKey } from '../common/fixtures.mjs';
import { KNOWN_BYTES_LONG, uint8concat, equalUint8Arrays } from '../common/quic/test-helpers.mjs';
import { TransformStream } from 'node:stream/web';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

const finished = Promise.withResolvers();

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.opened;
  const transformStream = new TransformStream();
  serverSession.closed.catch((err) => {
    // ignore the error
  });
  const sendStream = await serverSession.createBidirectionalStream({ body: transformStream.readable });
  // Now compare what we got

  strictEqual(sendStream.direction, 'bidi');
  sendStream.closed.catch(() => {
    // ignore
  });
  const writeToStream = async () => {
    const clientWritable = transformStream.writable;
    const writer = clientWritable.getWriter();
    for (const chunk of KNOWN_BYTES_LONG) {
      await writer.ready;
      await writer.write(chunk);
    }
    await writer.ready;
    await writer.close();
  };
  const readFromStream = async () => {
    const reader = sendStream.readable.getReader();
    const readChunks = [];
    while (true) {
      const { done, value } = await reader.read();
      if (value) {
        ok(value instanceof Uint8Array, 'Expects value to be a Uint8Array');
        readChunks.push(value);
      }
      if (done) break;
    }
    equalUint8Arrays(uint8concat(KNOWN_BYTES_LONG), uint8concat(readChunks));
  };
  await Promise.all([writeToStream(), readFromStream()]);
  serverSession.close();
  finished.resolve();
}, { keys, certs });

// The server must have an address to connect to after listen resolves.
ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;


clientSession.onstream = mustCall(async (stream) => {
  strictEqual(stream.direction, 'bidi', 'Expects an bidirectional stream');
  stream.closed.catch(() => {
    // ignore
  });
  const clientTransformStream = new TransformStream();
  stream.setOutbound(clientTransformStream.readable);
  await stream.readable.pipeTo(clientTransformStream.writable);
}, 1);

await finished.promise;
clientSession.closed.catch((err) => {
  // ignore the error
});
clientSession.close();
