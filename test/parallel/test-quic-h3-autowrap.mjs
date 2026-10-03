// Flags: --experimental-quic --no-warnings

// Test: sessions arrive wrapped in the application matching their ALPN,
// unless autoWrap is false. Servers know the negotiated protocol before
// surfacing a session, and clients offer exactly one.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, Http3Session } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const clientOpts = { servername: 'localhost', verifyPeer: 'manual' };

const isHttp3 = (session) => session instanceof Http3Session;

// Both sides wrap by ALPN.
{
  const seen = [];
  const endpoint = await listen(mustCall((session) => {
    seen.push(isHttp3(session));
    session.onerror = () => {};
  }, 3), {
    alpn: ['h3', 'h3-29', 'other'],
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  // One protocol: wrapped up front, before the handshake.
  const single = await connect(endpoint.address, { ...clientOpts, alpn: 'h3' });
  assert.ok(isHttp3(single));
  assert.throws(() => Http3Session.from(single.quicSession),
                { code: 'ERR_INVALID_STATE' });
  await single.opened;
  await single.close();

  // Draft ALPNs count as HTTP/3 too.
  const draft = await connect(endpoint.address, { ...clientOpts, alpn: 'h3-29' });
  assert.ok(isHttp3(draft));
  await draft.opened;
  await draft.close();

  // A non-HTTP/3 protocol stays a plain QuicSession on both sides.
  const other = await connect(endpoint.address, { ...clientOpts, alpn: 'other' });
  assert.ok(!isHttp3(other));
  await other.opened;
  await other.close();

  await endpoint.close();
  assert.deepStrictEqual(seen, [true, true, false]);
}

// Opting out gives the raw session on either side, to attach yourself.
{
  const endpoint = await listen(mustCall((quicSession) => {
    assert.ok(!isHttp3(quicSession));
    Http3Session.from(quicSession);
  }), {
    alpn: ['h3'],
    autoWrap: false,
    sni: { '*': { keys: [key], certs: [cert] } },
  });
  const quicSession = await connect(endpoint.address,
                                    { ...clientOpts, alpn: 'h3', autoWrap: false });
  assert.ok(!isHttp3(quicSession));
  const session = Http3Session.from(quicSession);
  await session.opened;
  await session.close();
  await endpoint.close();
}

for (const autoWrap of [1, 'yes', null]) {
  await assert.rejects(connect('127.0.0.1:1', { ...clientOpts, alpn: 'h3', autoWrap }),
                       { code: 'ERR_INVALID_ARG_TYPE', message: /options\.autoWrap/ });
}
