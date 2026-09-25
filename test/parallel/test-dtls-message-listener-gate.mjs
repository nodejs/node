// Flags: --experimental-dtls --no-warnings

// Test: incoming data is only copied into a JS Buffer when something is
// listening for it.
//
// The has_message_listener state flag was written by JS and never read by
// C++, so every datagram was copied onto the heap and dispatched into JS even
// with no onmessage handler, for the handler to drop it. The flag now gates
// that, in the same way has_keylog_listener gates keylog extraction.
//
// The allocation itself is not observable from JS, so what is pinned here is
// the part that would break if the gate were wrong: reading must continue
// regardless, or data would accumulate inside OpenSSL instead of being
// drained.

import { hasCrypto, skip } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const gotSession = Promise.withResolvers();
const endpoint = listen((session) => gotSession.resolve(session),
                        { cert, key, host: '127.0.0.1', port: 0 });

const client = connect('127.0.0.1', endpoint.address.port, {
  servername: 'agent1', ca: [ca],
});

await client.opened;
const session = await gotSession.promise;
await session.opened;

// Wait for a stat to reach an expected value, so the test does not depend on
// a fixed sleep. dgram send callbacks fire when the kernel takes the
// datagram, not when the peer has processed it.
async function waitForMessages(count) {
  for (let i = 0; i < 100; i++) {
    if (session.stats.messagesReceived >= count) return;
    await setTimeout(20);
  }
  assert.fail(`only ${session.stats.messagesReceived} of ${count} messages ` +
              'were read; data is not being drained');
}

// No listener: the data must still be read out of OpenSSL and counted.
for (let i = 0; i < 5; i++) client.send(`unheard-${i}`);
await waitForMessages(5);
assert.strictEqual(session.stats.messagesReceived, 5n);
assert.ok(session.stats.bytesReceived > 0n);

// Attaching a listener starts delivery.
const delivered = [];
session.onmessage = (data) => delivered.push(data.toString());

for (let i = 0; i < 3; i++) client.send(`heard-${i}`);
await waitForMessages(8);

// Nothing sent before the listener was attached should be replayed.
assert.deepStrictEqual(delivered, ['heard-0', 'heard-1', 'heard-2']);

// Removing it stops delivery but must not stop reading.
session.onmessage = null;
for (let i = 0; i < 4; i++) client.send(`unheard-again-${i}`);
await waitForMessages(12);

assert.strictEqual(session.stats.messagesReceived, 12n);
assert.strictEqual(delivered.length, 3);

await client.close();
await endpoint.close();
