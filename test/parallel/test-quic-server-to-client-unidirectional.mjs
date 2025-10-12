// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { ok, strictEqual, deepStrictEqual } from 'node:assert';
import { readKey } from '../common/fixtures.mjs';
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

// start demo data
// FIX ME: move the following to a central place
// if used in several tests
// taken from @fails-components/webtransport tests
// by the original author
function createBytesChunk(length) {
  const workArray = new Array(length / 2);
  for (let i = 0; i < length / 4; i++) {
    workArray[2 * i + 1] = length % 0xffff;
    workArray[2 * i] = i;
  }
  const helper = new Uint16Array(workArray);
  const toreturn = new Uint8Array(
    helper.buffer,
    helper.byteOffset,
    helper.byteLength
  );
  return toreturn;
}

// The number in the comments, help you identify the chunk, as it is the length first two bytes
// this is helpful, when debugging buffer passing
const KNOWN_BYTES_LONG = [
  createBytesChunk(60000), // 96, 234
  createBytesChunk(12), // 0, 12
  createBytesChunk(50000), // 195, 80
  createBytesChunk(1600), // 6, 64
  createBytesChunk(20000), // 78, 32
  createBytesChunk(30000), // 117, 48
];

// end demo data

function uint8concat(arrays) {
  const length = arrays.reduce((acc, curr) => acc + curr.length, 0);
  const result = new Uint8Array(length);
  let pos = 0;
  let array = 0;
  while (pos < length) {
    const curArr = arrays[array];
    const curLen = curArr.byteLength;
    const dest = new Uint8Array(result.buffer, result.byteOffset + pos, curLen);
    dest.set(curArr);
    array++;
    pos += curArr.byteLength;
  }
}

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.opened;
  const transformStream = new TransformStream();
  const sendStream = await serverSession.createUnidirectionalStream({ body: transformStream.readable });
  strictEqual(sendStream.direction, 'uni');
  const serverWritable = transformStream.writable;
  const writer = serverWritable.getWriter();
  for (const chunk of KNOWN_BYTES_LONG) {
    await writer.ready;
    await writer.write(chunk);
  }
  await writer.ready;
  await writer.close();
  serverSession.close();
}, { keys, certs });

// The server must have an address to connect to after listen resolves.
ok(serverEndpoint.address !== undefined);

const clientSession = await connect(serverEndpoint.address);

clientSession.onstream = mustCall(async (stream) => {
  strictEqual(stream.direction, 'uni', 'Expects an unidirectional stream');
  const reader = stream.readable.getReader();
  const readChunks = [];
  while (true) {
    const { done, value } = await reader.read();
    if (value) {
      ok(value instanceof Uint8Array, 'Expects value to be a Uint8Array');
      readChunks.push(value);
    }
    if (done) break;
  }
  // Now compare what we got
  deepStrictEqual(uint8concat(KNOWN_BYTES_LONG), uint8concat(readChunks));
  clientFinished.resolve();
}, 1);

await clientFinished.promise;
clientSession.close();
