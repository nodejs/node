// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
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

// The opened promise should resolve when the client finished reading
const clientFinished = Promise.withResolvers();


const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  const transformStream = new TransformStream();
  const sendStream = await serverSession.createUnidirectionalStream({ body: transformStream.readable });
  sendStream.closed.catch(() => {
    // ignore
  });
  assert.strictEqual(sendStream.direction, 'uni');
  const serverWritable = transformStream.writable;
  const writer = serverWritable.getWriter();
  for (const chunk of KNOWN_BYTES_LONG) {
    await writer.ready;
    await writer.write(chunk);
  }
  await writer.ready;
  await writer.close();
  serverSession.closed.catch((err) => {
    // ignore the error
  });
  serverSession.close();
}), { keys, certs });

// The server must have an address to connect to after listen resolves.
assert.ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address);

clientSession.onstream = mustCall(async (stream) => {
  assert.strictEqual(stream.direction, 'uni');
  const reader = stream.readable.getReader();
  const readChunks = [];
  while (true) {
    const { done, value } = await reader.read();
    if (value) {
      assert.ok(value instanceof Uint8Array, 'Expects value to be a Uint8Array');
      readChunks.push(value);
    }
    if (done) break;
  }
  stream.closed.catch(() => {
    // ignore
  });
  // Now compare what we got
  equalUint8Arrays(uint8concat(KNOWN_BYTES_LONG), uint8concat(readChunks));
  clientFinished.resolve();
}, 1);

await clientFinished.promise;
clientSession.closed.catch((err) => {
  // ignore the error
});
clientSession.close();
