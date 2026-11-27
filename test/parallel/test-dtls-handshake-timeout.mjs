// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: handshake timeout.
//
// OpenSSL abandons a handshake on its own, but only after
// DTLS1_TMO_ALERT_COUNT retransmits on a doubling backoff capped at 60s --
// around eight minutes. Until then the session holds a slot, so abandoned
// handshakes can sit against maxSessions indefinitely for the cost of
// starting them. This bounds that without altering the retransmit schedule,
// which has to keep its own timing.

import { hasCrypto, skip } from '../common/index.mjs';
import { setTimeout } from 'node:timers/promises';
import { once } from 'node:events';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dgram = await import('node:dgram');
const dtls = await import('node:dtls');

// The state and sessions views are not public API; they are reached here the
// way node:quic's tests reach theirs.
const {
  getDTLSEndpointState,
  getDTLSSessionState,
} = (await import('internal/dtls/dtls')).default;
const { connect, listen } = dtls;

const HOST = '127.0.0.1';
const KEY = Buffer.from('0123456789abcdef');
const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// Stall a handshake after the server has a session for it. A proxy between
// the two passes the server's first datagram -- the HelloVerifyRequest or
// HelloRetryRequest that carries the cookie -- and nothing after it. The
// client answers with the cookie, so the server validates the address and
// announces the session, and from then on neither side hears from the other.
// Both retransmit, and nothing but a deadline ends it.
async function stall(endpoint, options = {}) {
  const front = dgram.createSocket('udp4');
  const back = dgram.createSocket('udp4');
  let client;
  let passed = false;
  front.on('message', (msg, rinfo) => {
    client = rinfo;
    back.send(msg, endpoint.address.port, HOST);
  });
  back.on('message', (msg) => {
    if (passed) return;
    passed = true;
    front.send(msg, client.port, client.address);
  });
  front.bind(0, HOST);
  back.bind(0, HOST);
  await Promise.all([once(front, 'listening'), once(back, 'listening')]);

  const session = connect(HOST, front.address().port, {
    psk: { identity: 'device', key: KEY }, ...options,
  });
  session.closed.finally(() => {
    front.close();
    back.close();
  }).catch(() => {});
  return session;
}

function serve(options = {}, onsession = () => {}) {
  return listen(onsession, {
    psk: { 'device': KEY }, host: HOST, port: 0, ...options,
  });
}

for (const versions of [V12, V13]) {
  // The deadline is honoured, and close to exactly.
  for (const handshakeTimeout of [300, 700]) {
    const endpoint = serve({ handshakeTimeout, ...versions });
    const started = Date.now();

    const client = await stall(endpoint, { handshakeTimeout, ...versions });
    await assert.rejects(client.opened, { message: /handshake timeout/ });

    const elapsed = Date.now() - started;
    // The retransmit schedule doubles -- 1s, 3s, 7s -- so hitting a 300ms
    // deadline at all means the timer was clamped to it rather than left to
    // fire on the retransmit's own schedule.
    assert.ok(elapsed >= handshakeTimeout,
              `gave up after ${elapsed}ms, before the ${handshakeTimeout}ms ` +
              'deadline');
    assert.ok(elapsed < handshakeTimeout + 1000,
              `gave up after ${elapsed}ms, well past the ` +
              `${handshakeTimeout}ms deadline`);

    // Expiring is not the same as running out of retransmits. The deadline is
    // checked before DTLSv1_handle_timeout(), so the session stops rather
    // than sending again; reaching OpenSSL's own limit instead would mean
    // twelve more flights on the way out.
    assert.ok(Number(client.stats.retransmitCount) < 5,
              `expired after ${client.stats.retransmitCount} retransmits, ` +
              'which suggests OpenSSL\'s budget ran out rather than the ' +
              'deadline being honoured');

    await endpoint.close();
  }
}

// The slot is given back. This is the point of the exercise: without it an
// abandoned handshake occupies its place against maxSessions until the
// endpoint closes.
for (const versions of [V12, V13]) {
  const announced = [];
  const allAnnounced = Promise.withResolvers();
  const endpoint = serve({ handshakeTimeout: 300, maxSessions: 3, ...versions },
                         (session) => {
                           announced.push(session);
                           if (announced.length === 3) allAnnounced.resolve();
                         });

  const stalled = [];
  for (let i = 0; i < 3; i++) {
    const client = await stall(endpoint, {
      handshakeTimeout: 60000, ...versions,
    });
    client.opened.catch(() => {});
    stalled.push(client);
  }

  await allAnnounced.promise;
  assert.strictEqual(Number(getDTLSEndpointState(endpoint).sessionCount), 3);

  // Each server session expires on its own deadline.
  await Promise.allSettled(announced.map((session) => session.closed));
  assert.strictEqual(Number(getDTLSEndpointState(endpoint).sessionCount), 0);

  // And the endpoint is usable again.
  const good = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY }, ...versions,
  });
  await good.opened;
  await good.close();

  for (const client of stalled) client.destroy();
  await endpoint.close();
}

// A handshake that completes is not affected, and the deadline does not
// linger to fire at an established session.
for (const versions of [V12, V13]) {
  const endpoint = serve({ handshakeTimeout: 300, ...versions });

  const client = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY }, handshakeTimeout: 300, ...versions,
  });
  await client.opened;

  await setTimeout(600);
  assert.strictEqual(getDTLSSessionState(client).destroyed, false);

  // Still usable well after the deadline would have passed.
  const echoed = Promise.withResolvers();
  client.onmessage = (data) => echoed.resolve(data);
  const peer = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY }, ...versions,
  });
  await peer.opened;
  await peer.close();

  await client.close();
  await endpoint.close();
}

// Zero disables it, leaving OpenSSL's own retransmit budget as the only
// limit.
for (const versions of [V12, V13]) {
  const endpoint = serve({ handshakeTimeout: 0, ...versions });

  const client = await stall(endpoint, { handshakeTimeout: 0, ...versions });

  const pending = Symbol('pending');
  const outcome = await Promise.race([
    client.opened.then(() => 'opened', () => 'failed'),
    setTimeout(500, pending),
  ]);
  assert.strictEqual(outcome, pending);

  client.destroy();
  await endpoint.close();
}

// The default does not fire early. Checking the 60s default itself would
// mean waiting 60s, so this only pins that it is not something small.
for (const versions of [V12, V13]) {
  const endpoint = serve(versions);

  const client = await stall(endpoint, versions);

  const pending = Symbol('pending');
  const outcome = await Promise.race([
    client.opened.then(() => 'opened', () => 'failed'),
    setTimeout(500, pending),
  ]);
  assert.strictEqual(outcome, pending);

  client.destroy();
  await endpoint.close();
}

// It applies to a resumed handshake too, which is a different code path
// through the state machine but the same session lifecycle. A DTLS 1.3
// server issues its ticket after the handshake, so it is taken from
// onsessionticket rather than read when opened settles.
for (const versions of [V12, V13]) {
  const endpoint = listen(() => {}, {
    psk: { 'device': KEY }, host: HOST, port: 0, handshakeTimeout: 5000,
    ...versions,
  });

  const first = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY }, ...versions,
  });
  const issued = Promise.withResolvers();
  first.onsessionticket = (ticket) => issued.resolve(ticket);
  await first.opened;
  const ticket = await issued.promise;
  await first.close();

  const resumed = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY },
    session: ticket,
    handshakeTimeout: 5000,
    ...versions,
  });
  await resumed.opened;
  assert.strictEqual(resumed.reused, true);
  assert.strictEqual(getDTLSSessionState(resumed).destroyed, false);

  await resumed.close();
  await endpoint.close();
}

// Validation.
{
  for (const value of [-1, 1.5, 'soon', null, {}]) {
    assert.throws(() => listen(() => {}, {
      psk: { 'device': KEY }, host: HOST, port: 0, handshakeTimeout: value,
    }), { code: /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/ });
  }
}
