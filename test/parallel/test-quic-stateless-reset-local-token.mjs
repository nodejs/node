// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: locally issued stateless reset tokens are not treated as
// tokens received from the peer.
// The server hands out stateless reset tokens in NEW_CONNECTION_ID
// frames. Those tokens identify the server to its peer; they must not
// be looked up as reset tokens on incoming packets. Otherwise they
// outlive the server session, and a packet for an unknown connection
// ID that ends in one is dispatched to the destroyed session instead
// of being answered with a stateless reset.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { createSocket } from 'node:dgram';
import { Buffer } from 'node:buffer';
import { hkdfSync, randomBytes } from 'node:crypto';
import { dump } from 'node:stream/iter';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const resetTokenSecret = randomBytes(16);
const qlogChunks = [];
const serverQlogDone = Promise.withResolvers();
const serverExchanged = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    assert.ok(data.byteLength > 0);
    stream.writer.endSync();
    await stream.closed;
    serverExchanged.resolve(serverSession);
  });
}), {
  endpoint: { resetTokenSecret },
  qlog: true,
  onqlog(data, fin) {
    qlogChunks.push(data);
    if (fin) serverQlogDone.resolve();
  },
  onerror: mustNotCall(),
});

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
});
await clientSession.opened;

// A complete exchange, so the server has issued its NEW_CONNECTION_ID
// frames by the time the sessions are torn down.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('hello'),
});
await dump(stream);
await stream.closed;

const serverSession = await serverExchanged.promise;
clientSession.destroy();
serverSession.destroy();
await serverQlogDone.promise;

// Collect the stateless reset tokens the server sent to its peer.
const tokens = [];
const records = qlogChunks.join('').split('\x1e')
  .filter((s) => s.trim().length > 0)
  .map((s) => JSON.parse(s));
for (const { name, data } of records) {
  if (!name?.endsWith('packet_sent')) continue;
  for (const frame of data?.frames ?? []) {
    if (frame.frame_type === 'new_connection_id') {
      tokens.push(Buffer.from(frame.stateless_reset_token.data, 'hex'));
    }
  }
}
assert.ok(tokens.length > 0, 'Server should have issued NEW_CONNECTION_ID');

// A short header packet for an unknown connection ID, ending in `tail`.
// A stateless reset sent in reply ends in the token the endpoint derives
// from the packet's destination connection ID (the 20 bytes after the
// first byte, as the endpoint parses short headers), which tells the
// replies apart.
function shortHeaderPacket(tail) {
  const packet = randomBytes(64);
  packet[0] = 0x40;  // Short header, fixed bit set.
  tail.copy(packet, packet.length - tail.length);
  const dcid = packet.subarray(1, 21);
  const replyToken = Buffer.from(
    hkdfSync('sha256', resetTokenSecret, dcid, 'stateless_reset', 16));
  return { packet, replyToken };
}

const localToken = shortHeaderPacket(tokens[0]);
const control = shortHeaderPacket(randomBytes(16));

const { address, family, port } = serverEndpoint.address;
const socket = createSocket(family === 'ipv6' ? 'udp6' : 'udp4');
const replies = [];
const controlAnswered = Promise.withResolvers();
socket.on('message', (msg) => {
  const token = msg.subarray(msg.length - 16);
  replies.push(token);
  if (token.equals(control.replyToken)) controlAnswered.resolve();
});

// Both packets go out on one socket, so the server receives and answers
// them in order: by the time the control packet is answered, the reply
// to the packet ending in a locally issued token has arrived as well.
socket.send(localToken.packet, port, address);
socket.send(control.packet, port, address);
await controlAnswered.promise;

assert.ok(replies.some((token) => token.equals(localToken.replyToken)),
          'Packet ending in a local token should get a stateless reset');

socket.close();
await serverEndpoint.close();
