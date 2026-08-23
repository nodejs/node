// Flags: --experimental-dtls --no-warnings

// Test: maxSessions and maxSessionsPerHost bound the server session table.
//
// Each accepted session owns an SSL, two BIOs and a retransmit timer, and
// nothing used to bound how many of them a peer could accumulate.

import { hasCrypto, skip } from '../common/index.mjs';
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

// Connects and resolves true if the handshake got through. A refused peer is
// answered with silence rather than an alert, so it retransmits and then
// fails; keep the wait short so the test does not sit through the full DTLS
// retransmit schedule.
async function tryConnect(port, timeout = 1500) {
  const client = connect('127.0.0.1', port, { servername: 'agent1', ca: [ca] });
  const timer = new Promise((resolve) => {
    setTimeout(resolve, timeout).unref();
  });
  const opened = await Promise.race([
    client.opened.then(() => true, () => false),
    timer.then(() => false),
  ]);
  return { opened, client };
}

// Closing a client resolves as soon as the client side is down; the server
// only drops its half once the close_notify arrives.
async function waitForSessionCount(server, expected) {
  for (let i = 0; i < 100; i++) {
    if (server.state.sessionCount === expected) return;
    await new Promise((resolve) => {
      setTimeout(resolve, 10).unref();
    });
  }
  assert.strictEqual(server.state.sessionCount, expected);
}

// --- maxSessions ---------------------------------------------------------

{
  const server = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0, maxSessions: 2,
  });
  const { port } = server.address;

  const first = await tryConnect(port);
  const second = await tryConnect(port);
  assert.strictEqual(first.opened, true);
  assert.strictEqual(second.opened, true);
  assert.strictEqual(server.state.sessionCount, 2);

  // The table is full, so the third is refused before anything is allocated.
  const third = await tryConnect(port);
  assert.strictEqual(third.opened, false);
  assert.strictEqual(server.state.sessionCount, 2);
  assert.ok(server.stats.serverRefusedCount > 0n,
            'expected the refusal to be counted');

  // Freeing a slot lets a new peer in, so the cap is a live bound and not a
  // one-way latch.
  await first.client.close();
  await waitForSessionCount(server, 1);

  const fourth = await tryConnect(port);
  assert.strictEqual(fourth.opened, true);

  await fourth.client.close();
  await second.client.close();
  try {
    await third.client.close();
  } catch {
    // Never opened.
  }
  await server.close();
}

// --- maxSessionsPerHost --------------------------------------------------

// Every client here is on 127.0.0.1, so the per-host cap binds first even
// though the overall cap is far higher.
{
  const server = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
    maxSessions: 100, maxSessionsPerHost: 1,
  });
  const { port } = server.address;

  const first = await tryConnect(port);
  assert.strictEqual(first.opened, true);

  const second = await tryConnect(port);
  assert.strictEqual(second.opened, false);
  assert.strictEqual(server.state.sessionCount, 1);

  // The per-host count is released along with the session.
  await first.client.close();
  await waitForSessionCount(server, 0);
  const third = await tryConnect(port);
  assert.strictEqual(third.opened, true);

  await third.client.close();
  try {
    await second.client.close();
  } catch {
    // Never opened.
  }
  await server.close();
}

// --- validation ----------------------------------------------------------

for (const bad of [-1, 1.5, '10']) {
  assert.throws(
    () => listen(() => {}, {
      cert, key, host: '127.0.0.1', port: 0, maxSessions: bad,
    }),
    { code: /^ERR_(OUT_OF_RANGE|INVALID_ARG_TYPE)$/ });
}
