// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Quic maxstreamdata updates on pure quic
// Client sends a body that precisely fills the window size,
// and verifies that it is data transfer is not stalled.

import { hasQuic, skip } from '../common/index.mjs';
import { readFile } from 'node:fs/promises';
import { setTimeout as sleep } from 'node:timers/promises';

if (!hasQuic) {
  skip('QUIC is not enabled');
}
const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { drainableProtocol } = await import('stream/iter');

const keys = 'test/fixtures/keys';
const key = createPrivateKey(await readFile(`${keys}/agent1-key.pem`));
const cert = await readFile(`${keys}/agent1-cert.pem`);

const WINDOW = 4096;
// Fills the window exactly: HTTP/3 spends 11 of those bytes on framing (8 for
// the HEADERS frame below, 3 for the DATA frame header). The send buffer then
// empties at the same moment the window reaches zero, leaving nothing in
// flight to ack. Any other size leaves bytes queued, and the ack for those
// wakes the writer instead, hiding the bug.
const BODY = WINDOW;

let letServerRead;
const serverMayRead = new Promise((resolve) => { letServerRead = resolve; });

const endpoint = await listen((session) => {
  session.onstream = async (stream) => {
    await serverMayRead;
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream) { /* reading extends the window */ }
  };
}, {
  alpn: 'foo',
  sni: { '*': { keys: [key], certs: [cert] } },
  transportParams: {
    initialMaxStreamDataBidiRemote: WINDOW,
    initialMaxData: 1024 * 1024,
  }
});

const session = await connect(endpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  alpn: 'foo'
});
await session.opened;

// Budget well above the window, so the window is what stops the writer.
const stream = await session.createBidirectionalStream({ budget: 1024 * 1024 });

const writer = stream.writer;
writer.writeSync(new Uint8Array(BODY));

// Long enough for every byte to be acked. The peer acks as data arrives,
// whether or not its application has read any of it, so by now the window is
// exhausted, the send buffer is empty, and no further ACK can arrive.
await sleep(500);

const watchdog = setTimeout(() => {
  console.error('STALLED: no drain after MAX_STREAM_DATA');
  process.exit(1);
}, 5000);

letServerRead();                 // Extend the window, with no ack attached
await writer[drainableProtocol]();

clearTimeout(watchdog);
process.exit(0);
