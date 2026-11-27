// Flags: --experimental-dtls --no-warnings

// Test: session resumption -- session.session, the `session` option on
// connect(), session.reused, and ticketKeys.
//
// Resumption matters more here than it does over TCP. A full DTLS handshake's
// Certificate flight is fragmented across several datagrams and any one of
// them being lost costs a retransmission timeout; a resumed handshake skips
// it. Measured on loopback, the server sends 1850 bytes in 4 packets for a
// full handshake against 280 bytes in 3 for a resumed one.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { inspect } from 'node:util';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { randomBytes, X509Certificate } = await import('node:crypto');
const { connect, createSecureContext, listen } = await import('node:dtls');

const key = (name) => fixtures.readKey(name).toString();
const cert = key('agent1-cert.pem');
const privateKey = key('agent1-key.pem');

const serverOptions = {
  cert, key: privateKey, host: '127.0.0.1', port: 0,
};

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// Connects, and returns the session with a ticket to resume it from. A DTLS
// 1.2 server issues its ticket during the handshake, so session.session has
// it once opened settles. A DTLS 1.3 server sends tickets after the
// handshake, and the first is taken from onsessionticket.
async function ticketed(port, versions, options = {}) {
  const session = connect('127.0.0.1', port, {
    rejectUnauthorized: false, ...options, ...versions,
  });
  const issued = Promise.withResolvers();
  session.onsessionticket = (ticket) => issued.resolve(ticket);
  await session.opened;
  const ticket = versions === V12 ? session.session : await issued.promise;
  return { session, ticket };
}

// A session can be carried from one connection to the next.
for (const versions of [V12, V13]) {
  const endpoint = listen(() => {}, { ...serverOptions, ...versions });

  const { session: first, ticket } =
    await ticketed(endpoint.address.port, versions);

  assert.strictEqual(first.reused, false);
  assert.ok(Buffer.isBuffer(ticket));
  assert.ok(ticket.length > 0);
  await first.close();

  const { session: second, ticket: next } =
    await ticketed(endpoint.address.port, versions, { session: ticket });
  assert.strictEqual(second.reused, true);

  // A resumed connection can itself be resumed from.
  assert.ok(Buffer.isBuffer(next));
  await second.close();

  await endpoint.close();
}

// Resumption skips the Certificate flight, which is the point of it.
for (const versions of [V12, V13]) {
  const endpoint = listen(() => {}, { ...serverOptions, ...versions });
  const sent = () => Number(endpoint.stats.bytesSent);

  const beforeFull = sent();
  const full = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });
  const issued = Promise.withResolvers();
  full.onsessionticket = (ticket) => issued.resolve(ticket);
  await full.opened;
  const fullBytes = sent() - beforeFull;
  const ticket = await issued.promise;
  await full.close();

  const beforeResumed = sent();
  const resumed = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, session: ticket, ...versions,
  });
  await resumed.opened;
  const resumedBytes = sent() - beforeResumed;

  // Read before closing: like authorized, this reports false once the
  // handle is gone.
  assert.strictEqual(resumed.reused, true);
  await resumed.close();

  if (versions === V12) {
    assert.ok(resumedBytes < fullBytes / 2,
              `resumed handshake sent ${resumedBytes} bytes, ` +
              `full sent ${fullBytes}; expected well under half`);
  } else {
    // A DTLS 1.3 server sends its key share either way, and the default
    // post-quantum hybrid group's is over a kilobyte, so the saving is the
    // certificate rather than half.
    const certBytes = new X509Certificate(cert).raw.length;
    assert.ok(fullBytes - resumedBytes > certBytes,
              `resumed handshake sent ${resumedBytes} bytes, ` +
              `full sent ${fullBytes}; expected at least the ` +
              `${certBytes} byte certificate less`);
  }

  await endpoint.close();
}

// The cookie exchange still runs for a resumed handshake. Resumption must not
// become a way around the address validation that the endpoint's DoS
// protection depends on.
for (const versions of [V12, V13]) {
  const endpoint = listen(() => {}, { ...serverOptions, ...versions });

  const { session: first, ticket } =
    await ticketed(endpoint.address.port, versions);
  await first.close();

  const before = Number(endpoint.stats.packetsReceived);
  const second = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, session: ticket, ...versions,
  });
  await second.opened;
  const received = Number(endpoint.stats.packetsReceived) - before;

  // At least two: the first ClientHello is answered with a
  // HelloVerifyRequest or HelloRetryRequest and the client has to send
  // another carrying the cookie. One would mean it had been bypassed.
  assert.ok(received >= 2,
            `resumed handshake cost the server ${received} packets; ` +
            'fewer than 2 means the cookie exchange was skipped');
  assert.strictEqual(second.reused, true);

  await second.close();
  await endpoint.close();
}

// A session is bound to the identity it was authenticated for and refused
// anywhere else. A resumed handshake does not re-verify the peer's
// certificate, so replaying a session against another host would skip
// verification while appearing to succeed. This is CVE-2026-48934, fixed in
// node:tls; the same hazard applies to any API that lets a session travel
// between connections.
for (const versions of [V12, V13]) {
  const endpoint = listen(() => {}, { ...serverOptions, ...versions });

  const { session: first, ticket } = await ticketed(
    endpoint.address.port, versions, { servername: 'host-a' });
  await first.close();

  // The identity it was issued for: allowed.
  const same = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, servername: 'host-a', session: ticket,
    ...versions,
  });
  await same.opened;
  assert.strictEqual(same.reused, true);
  await same.close();

  // Any other: refused before OpenSSL sees it.
  assert.throws(() => connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, servername: 'host-b', session: ticket,
    ...versions,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /authenticated for 'host-a'.*cannot be reused for 'host-b'/,
  });

  // A blob that did not come from session.session cannot be vouched for,
  // because nothing records which identity it belongs to.
  assert.throws(() => connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, session: randomBytes(64),
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  await endpoint.close();
}

// An unusable session falls back to a full handshake rather than failing.
for (const versions of [V12, V13]) {
  const first = listen(() => {}, { ...serverOptions, ...versions });
  const { session: client, ticket } =
    await ticketed(first.address.port, versions);
  await client.close();
  await first.close();

  // A different endpoint has a different, randomly generated ticket key, so
  // it cannot decrypt the ticket.
  const second = listen(() => {}, { ...serverOptions, ...versions });
  const stale = connect('127.0.0.1', second.address.port, {
    rejectUnauthorized: false, session: ticket, ...versions,
  });
  await stale.opened;
  assert.strictEqual(stale.reused, false);
  await stale.close();
  await second.close();
}

// ticketKeys makes tickets portable between endpoints, which is what makes
// them survive a restart or work across a cluster.
for (const versions of [V12, V13]) {
  const ticketKeys = randomBytes(80);

  const a = listen(() => {}, { ...serverOptions, ticketKeys, ...versions });
  const b = listen(() => {}, { ...serverOptions, ticketKeys, ...versions });

  const { session: first, ticket } = await ticketed(a.address.port, versions);
  await first.close();

  const second = connect('127.0.0.1', b.address.port, {
    rejectUnauthorized: false, session: ticket, ...versions,
  });
  await second.opened;
  assert.strictEqual(second.reused, true);
  await second.close();

  await a.close();
  await b.close();
}

// ticketKeys validation. The length is OpenSSL's rather than ours, so the
// error reports what it asked for.
{
  assert.throws(() => createSecureContext({
    cert, key: privateKey, isServer: true, ticketKeys: randomBytes(48),
  }), { code: 'ERR_INVALID_ARG_VALUE', message: /exactly 80 bytes/ });

  for (const value of ['keys', 42, null, {}]) {
    assert.throws(() => createSecureContext({
      cert, key: privateKey, isServer: true, ticketKeys: value,
    }), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // It belongs to the context, so it cannot be passed alongside one.
  const context = createSecureContext({
    cert, key: privateKey, isServer: true,
  });
  assert.throws(() => listen(() => {}, {
    secureContext: context, ticketKeys: randomBytes(80),
    host: '127.0.0.1', port: 0,
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// A server session has no identity to bind a blob to, so it offers none.
for (const versions of [V12, V13]) {
  const gotSession = Promise.withResolvers();
  const endpoint = listen((s) => gotSession.resolve(s),
                          { ...serverOptions, ...versions });

  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false, ...versions,
  });
  await client.opened;
  const session = await gotSession.promise;
  await session.opened;

  assert.strictEqual(session.session, undefined);
  assert.strictEqual(session.reused, false);

  await client.close();
  await endpoint.close();
}

// A session that is not a Buffer is a type error, not a value error.
//
// The Buffer check was folded in with the prefix and length checks and all
// four reported ERR_INVALID_ARG_VALUE, so passing a string got the code that
// means the type was right and the contents were wrong.
{
  for (const value of ['a string', 7, {}, null, []]) {
    assert.throws(() => connect('127.0.0.1', 4433, { session: value }), {
      code: 'ERR_INVALID_ARG_TYPE',
    }, `session: ${inspect(value)}`);
  }

  // A Buffer that is not one of ours is still a value error: the right type,
  // contents this cannot resume.
  for (const value of [Buffer.alloc(0), Buffer.alloc(40), Buffer.from('xx')]) {
    assert.throws(() => connect('127.0.0.1', 4433, { session: value }), {
      code: 'ERR_INVALID_ARG_VALUE',
    }, `session: ${inspect(value)}`);
  }
}
