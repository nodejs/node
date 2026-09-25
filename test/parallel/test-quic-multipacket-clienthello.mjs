// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// A large post-quantum key share splits the ClientHello across QUIC Initial
// packets. The server must accept the incomplete first packet before ALPN has
// selected its application, then complete the handshake and process streams.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { listen, connect } = await import('node:quic');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const alpn = 'quic-multipacket-clienthello';
const groups = 'X25519MLKEM768';
const streamReceived = Promise.withResolvers();

const endpoint = await listen(mustCall(async (session) => {
  const info = await session.opened;
  assert.strictEqual(session.alpnProtocol, alpn);
  assert.strictEqual(info.cipherVersion, 'TLSv1.3');

  session.onstream = mustCall(async (stream) => {
    assert.strictEqual(
      Buffer.from(await bytes(stream)).toString(),
      'application event survived',
    );
    stream.writer.endSync();
    await stream.closed;
    streamReceived.resolve();
  });
}), {
  host: '127.0.0.1',
  port: 0,
  alpn: [alpn],
  groups,
  sni: { '*': { keys: [key], certs: [cert] } },
});

const session = await connect(endpoint.address, {
  alpn,
  groups,
  servername: 'localhost',
  verifyPeer: 'manual',
});

await session.opened;
const stream = await session.createBidirectionalStream({
  body: 'application event survived',
});
await Promise.all([stream.closed, streamReceived.promise]);
await session.close();
await endpoint.close();
