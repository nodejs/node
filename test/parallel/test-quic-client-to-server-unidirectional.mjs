// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { ok, strictEqual, deepStrictEqual } from 'node:assert';
import { readKey } from '../common/fixtures.mjs';
import { KNOWN_BYTES_LONG, uint8concat } from '../common/quic/test-helpers.mjs';
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
const serverFinished = Promise.withResolvers();


const serverEndpoint = await listen(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    strictEqual(stream.direction, 'uni', 'Expects an unidirectional stream');
    const reader = stream.readable.getReader();
    const readChunks = [];
    let readc = 0;
    while (true) {
      const { done, value } = await reader.read();
      // if (readc > 20) throw new Error("after read " + readc);
      if (value) {
        ok(value instanceof Uint8Array, 'Expects value to be a Uint8Array');
        readChunks.push(value);
      }
      if (done) break;
      readc++;
    }
    stream.closed.catch(() => {
      // ignore
    });
    // Now compare what we got
    deepStrictEqual(uint8concat(KNOWN_BYTES_LONG), uint8concat(readChunks));
    serverFinished.resolve();
  }, 1);

  await serverFinished.promise;
  serverSession.closed.catch((err) => {
    // ignore the error
  });
  serverSession.close();
}, { keys, certs });

// The server must have an address to connect to after listen resolves.
ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;


const transformStream = new TransformStream();
const sendStream = await clientSession.createUnidirectionalStream({ body: transformStream.readable });
sendStream.closed.catch(() => {
  // ignore
});
strictEqual(sendStream.direction, 'uni');
const clientWritable = transformStream.writable;
const writer = clientWritable.getWriter();
let run = 0
for (const chunk of KNOWN_BYTES_LONG) {
  await writer.ready;
  await writer.write(chunk);
  run++;
}
await writer.ready;
await writer.close();
clientSession.closed.catch((err) => {
  // ignore the error
});
clientSession.close();
await serverFinished.promise;