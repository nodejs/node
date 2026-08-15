// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Quic maxdata updates on http/3
// Client sends a body that precisely fills the session window size,
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
// Fills the window exactly:
// considers all framing including some initial session capsules
const BODY = WINDOW - 38;

let letServerRead;
const serverMayRead = new Promise((resolve) => { letServerRead = resolve; });

const endpoint = await listen((session) => {
  session.onstream = async (stream) => {
    await serverMayRead;
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream) { /* reading extends the window */ }
  };
}, {
  sni: { '*': { keys: [key], certs: [cert] } },
  transportParams: {
    initialMaxStreamDataBidiRemote: 1024 * 1024,  // Make sure maxstreamdata does not block
    initialMaxData: WINDOW,
  },
  onheaders() { this.sendHeaders({ ':status': '200' }); },
});

const session = await connect(endpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
});
await session.opened;

// Budget well above the window, so the window is what stops the writer.
const stream = await session.createBidirectionalStream({ budget: 1024 * 1024 });
stream.sendHeaders({
  ':method': 'POST',
  ':path': '/',
  ':scheme': 'https',
  ':authority': 'localhost',
}, { terminal: false });

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
