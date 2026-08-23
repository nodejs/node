// Flags: --experimental-dtls --no-warnings

// Test: handshake timeout.
//
// OpenSSL abandons a handshake on its own, but only after
// DTLS1_TMO_ALERT_COUNT retransmits on a doubling backoff capped at 60s --
// around eight minutes. Until then the session holds a slot, so abandoned
// handshakes can sit against maxSessions indefinitely for the cost of
// starting them. This bounds that without altering the retransmit schedule,
// which has to keep its own timing.

import { hasCrypto, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');
const { connect, listen } = dtls;

const HOST = '127.0.0.1';
const KEY = Buffer.from('0123456789abcdef');
const WRONG = Buffer.from('fedcba9876543210');

// A wrong pre-shared key is the cheapest way to stall a handshake: the
// identity only names the key, so both sides derive different secrets and
// then discard each other's records without replying, as DTLS requires
// (RFC 6347 section 4.1.2.1). Neither peer is ever told, and both retransmit.
function stall(endpoint, options = {}) {
  return connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: WRONG }, ...options,
  });
}

function serve(options = {}) {
  return listen(() => {}, {
    psk: { 'device': KEY }, host: HOST, port: 0, ...options,
  });
}

// The deadline is honoured, and close to exactly.
{
  for (const handshakeTimeout of [300, 700]) {
    const endpoint = serve({ handshakeTimeout });
    const started = Date.now();

    const client = stall(endpoint, { handshakeTimeout });
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
{
  const endpoint = serve({ handshakeTimeout: 300, maxSessions: 3 });

  const stalled = [];
  for (let i = 0; i < 3; i++) {
    const client = stall(endpoint, { handshakeTimeout: 60000 });
    client.opened.catch(() => {});
    stalled.push(client);
  }

  await new Promise((resolve) => setTimeout(resolve, 150));
  assert.strictEqual(Number(endpoint.state.sessionCount), 3);

  await new Promise((resolve) => setTimeout(resolve, 600));
  assert.strictEqual(Number(endpoint.state.sessionCount), 0);

  // And the endpoint is usable again.
  const good = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY },
  });
  await good.opened;
  await good.close();

  for (const client of stalled) client.destroy();
  await endpoint.close();
}

// A handshake that completes is not affected, and the deadline does not
// linger to fire at an established session.
{
  const endpoint = serve({ handshakeTimeout: 300 });

  const client = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY }, handshakeTimeout: 300,
  });
  await client.opened;

  await new Promise((resolve) => setTimeout(resolve, 600));
  assert.strictEqual(client.state.destroyed, false);

  // Still usable well after the deadline would have passed.
  const echoed = Promise.withResolvers();
  client.onmessage = (data) => echoed.resolve(data);
  const peer = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY },
  });
  await peer.opened;
  await peer.close();

  await client.close();
  await endpoint.close();
}

// Zero disables it, leaving OpenSSL's own retransmit budget as the only
// limit.
{
  const endpoint = serve({ handshakeTimeout: 0 });

  const client = stall(endpoint, { handshakeTimeout: 0 });

  const pending = Symbol('pending');
  const outcome = await Promise.race([
    client.opened.then(() => 'opened', () => 'failed'),
    new Promise((resolve) => setTimeout(resolve, 500, pending)),
  ]);
  assert.strictEqual(outcome, pending);

  client.destroy();
  await endpoint.close();
}

// The default does not fire early. Checking the 60s default itself would
// mean waiting 60s, so this only pins that it is not something small.
{
  const endpoint = serve();

  const client = stall(endpoint);

  const pending = Symbol('pending');
  const outcome = await Promise.race([
    client.opened.then(() => 'opened', () => 'failed'),
    new Promise((resolve) => setTimeout(resolve, 500, pending)),
  ]);
  assert.strictEqual(outcome, pending);

  client.destroy();
  await endpoint.close();
}

// It applies to a resumed handshake too, which is a different code path
// through the state machine but the same session lifecycle.
{
  const endpoint = listen(() => {}, {
    psk: { 'device': KEY }, host: HOST, port: 0, handshakeTimeout: 5000,
  });

  const first = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY },
  });
  await first.opened;
  const ticket = first.session;
  await first.close();

  const resumed = connect(HOST, endpoint.address.port, {
    psk: { identity: 'device', key: KEY },
    session: ticket,
    handshakeTimeout: 5000,
  });
  await resumed.opened;
  assert.strictEqual(resumed.state.destroyed, false);

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
