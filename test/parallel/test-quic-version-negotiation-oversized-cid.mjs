// Flags: --experimental-quic --no-warnings

// Regression test for an unauthenticated remote crash in the QUIC Version
// Negotiation path.
//
// ngtcp2_pkt_decode_version_cid() does not clamp the connection ID lengths
// to NGTCP2_MAX_CIDLEN (20) when the packet's version is unsupported -- it
// returns the raw single-byte length fields from the wire, which can be up
// to 255. Endpoint::Receive() used to build CID objects (each backed by a
// fixed 20-byte buffer) directly from those lengths before any bound check,
// so a single crafted UDP datagram with an oversized DCID/SCID length would
// overflow the buffer and abort the process before the handshake.
//
// This test sends such a datagram directly with node:dgram and asserts the
// endpoint drops it without crashing, while a well-formed unsupported-version
// datagram still produces exactly one Version Negotiation response.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createSocket } = await import('node:dgram');
const { listen } = await import('../common/quic.mjs');

// A long-header QUIC packet must be at least NGTCP2_MAX_UDP_PAYLOAD_SIZE
// (1200) bytes for an unsupported version to decode as a VN trigger.
const PACKET_SIZE = 1200;

// Build a QUIC long-header packet with an unsupported version and the given
// DCID/SCID lengths. Lengths greater than 20 are only expressible because the
// on-wire length field is a single byte (0-255).
function buildLongHeaderPacket(dcidLen, scidLen) {
  const packet = Buffer.alloc(PACKET_SIZE);
  // Header form bit (0x80) + fixed bit (0x40).
  packet[0] = 0xc0;
  // Version 0x0a0a0a0a: nonzero (so it is not a real VN packet) and not a
  // version Node supports, which forces the NGTCP2_ERR_VERSION_NEGOTIATION
  // decode path.
  packet.writeUInt32BE(0x0a0a0a0a, 1);
  packet[5] = dcidLen;          // DCID length byte
  // DCID bytes occupy [6, 6 + dcidLen); SCID length byte follows them.
  packet[6 + dcidLen] = scidLen;
  // Remaining bytes stay zero-filled as padding to reach PACKET_SIZE.
  return packet;
}

// No handshake ever completes: the test only sends raw datagrams, so the
// session callback must never fire.
const serverEndpoint = await listen(mustNotCall());
const { address, port } = serverEndpoint.address;

const socket = createSocket('udp4');

function send(packet) {
  return new Promise((resolve, reject) => {
    socket.send(packet, port, address, (err) => {
      if (err) reject(err);
      else resolve();
    });
  });
}

// 1. Oversized DCID (21 > NGTCP2_MAX_CIDLEN). Before the fix this aborted the
//    process. After the fix it must be dropped silently.
await send(buildLongHeaderPacket(21, 0));

// 2. A well-formed unsupported-version packet with valid (<= 20 byte) CIDs.
//    This must still elicit exactly one Version Negotiation response, proving
//    the fix did not break legitimate version negotiation.
await send(buildLongHeaderPacket(8, 8));

// Poll until the valid packet has been processed into a VN response.
const deadline = Date.now() + 2000;
while (serverEndpoint.stats.versionNegotiationCount === 0n) {
  if (Date.now() > deadline) break;
  await new Promise((resolve) => setTimeout(resolve, 25));
}

// Exactly one VN response: the oversized packet was dropped (not crashed, not
// negotiated), the valid packet was negotiated.
assert.strictEqual(serverEndpoint.stats.versionNegotiationCount, 1n);

socket.close();
await serverEndpoint.close();
